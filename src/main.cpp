/*
    Name:       APRS-ESP is APRS Internet Gateway / Tracker / Digipeater
    Created:    2022-10-10
    Author:     Ernest (ErNis) / LY3PH
    License:    GNU General Public License v3.0
    Includes code from:
                https://github.com/nakhonthai/ESP32IGate
                https://github.com/sh123/aprs_tracker
*/

#include <Arduino.h>
#include "main.h"
#include "utilities.h"
#include "pkgList.h"
#include "config.h"
#include <LibAPRSesp.h>
#include <limits.h>
#include <KISS.h>
#include "webservice.h"
#include <WiFiUdp.h>
#include "ESP32Ping.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include "cppQueue.h"
#include "BluetoothSerial.h"
#ifdef USE_GPS
#include "gps.h"
#endif
#include "digirepeater.h"
#include "igate.h"
#include <pbuf.h>
#include <parse_aprs.h>
#include <WiFiUdp.h>

#ifdef USE_RF
#include "rfModem.h"
#endif

#ifdef USE_GPS
#include "TinyGPSPlus.h"
#endif

#ifdef USE_ROTARY
#include <Rotary.h>
#endif

#include "rotaryProc.h"

#ifdef USE_SCREEN
#include "Wire.h"
#include "Adafruit_GFX.h"
#if defined(USE_SCREEN_SSD1306)
#include "Adafruit_SSD1306.h"
#elif defined(USE_SCREEN_SH1106)
#include "Adafruit_SH1106.h"
#endif
#include "oled.h"
#endif

#include <WiFiClientSecure.h>

#include "AFSK.h"

#ifdef SDCARD
#include <SPI.h> //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)
#include "FS.h"
#include "SPIFFS.h"
#define SDCARD_CS 13
#define SDCARD_CLK 14
#define SDCARD_MOSI 15
#define SDCARD_MISO 2
#endif

#ifdef USE_RF
HardwareSerial SerialRF(SERIAL_RF_UART);
#endif

const char *str_status[] = {
    "IDLE_STATUS",
    "NO_SSID_AVAIL",
    "SCAN_COMPLETED",
    "CONNECTED",
    "CONNECT_FAILED",
    "CONNECTION_LOST",
    "DISCONNECTED"
};

time_t systemUptime = 0;
time_t wifiUptime = 0;

boolean KISS = false;
bool aprsUpdate = false;

boolean gotPacket = false;
AX25Msg incomingPacket;

bool lastPkg = false;
bool afskSync = false;
String lastPkgRaw = "";
float dBV = 0;
int mVrms = 0;

cppQueue PacketBuffer(sizeof(AX25Msg), 5, IMPLEMENTATION);
cppQueue dispBuffer(300, 5, IMPLEMENTATION);

statusType status;
RTC_DATA_ATTR igateTLMType igateTLM;
RTC_DATA_ATTR txQueueType txQueue[PKGTXSIZE];

extern RTC_DATA_ATTR uint8_t digiCount;

Configuration config;

TaskHandle_t taskNetworkHandle;
TaskHandle_t taskAPRSHandle;

TelemetryType Telemetry[TLMLISTSIZE];

#if defined(USE_TNC)
HardwareSerial SerialTNC(SERIAL_TNC_UART);
#endif

#if defined(USE_GPS)
TinyGPSPlus gps;
HardwareSerial SerialGPS(SERIAL_GPS_UART);
#endif

// BluetoothSerial SerialBT;

#if defined(USE_SCREEN_SSD1306)
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST_PIN);
#elif defined(USE_SCREEN_SH1106)
Adafruit_SH1106 display(OLED_SDA_PIN, OLED_SCL_PIN);
#endif

#ifdef USE_ROTARY
Rotary rotary = Rotary(PIN_ROT_CLK, PIN_ROT_DT, PIN_ROT_BTN);
#endif

struct pbuf_t aprs;
ParseAPRS aprsParse;

// Set your Static IP address for wifi AP
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 254);
IPAddress subnet(255, 255, 255, 0);

int pkgTNC_count = 0;

unsigned long NTP_Timeout;
unsigned long pingTimeout;

static long scrUpdTMO = 0;
static long gpsUpdTMO = 0;

const char *lastTitle = "LAST HERT";

int tlmList_Find(char *call) {
    int i;
    for (i = 0; i < TLMLISTSIZE; i++) {
        if (strstr(Telemetry[i].callsign, call) != NULL)
            return i;
    }
    return -1;
}

int tlmListOld() {
    int i, ret = 0;
    time_t minimum = Telemetry[0].time;
    for (i = 1; i < TLMLISTSIZE; i++) {
        if (Telemetry[i].time < minimum) {
            minimum = Telemetry[i].time;
            ret = i;
        }
        if (Telemetry[i].time > now())
            Telemetry[i].time = 0;
    }
    return ret;
}

bool pkgTxUpdate(const char *info, int delay) {
    char *ecs = strstr(info, ">");
    if (ecs == NULL) return false;
    // Replace
    for (int i = 0; i < PKGTXSIZE; i++) {
        if (txQueue[i].Active) {
            if (!(strncmp(&txQueue[i].Info[0], info, info - ecs))) {
                strcpy(&txQueue[i].Info[0], info);
                txQueue[i].Delay = delay;
                txQueue[i].timeStamp = millis();
                return true;
            }
        }
    }

    // Add
    for (int i = 0; i < PKGTXSIZE; i++) {
        if (txQueue[i].Active == false) {
            strcpy(&txQueue[i].Info[0], info);
            txQueue[i].Delay = delay;
            txQueue[i].Active = true;
            txQueue[i].timeStamp = millis();
            break;
        }
    }
    return true;
}

bool pkgTxSend() {
    for (int i = 0; i < PKGTXSIZE; i++) {
        if (txQueue[i].Active) {
            int decTime = millis() - txQueue[i].timeStamp;
            if (decTime > txQueue[i].Delay) {
#ifdef USE_RF
                digitalWrite(POWER_PIN, config.rf_power);  // RF Power LOW
#endif
                APRS_setPreamble(APRS_PREAMBLE);
                APRS_setTail(APRS_TAIL);
                APRS_sendTNC2Pkt(String(txQueue[i].Info));  // Send packet to RF
                txQueue[i].Active = false;
                igateTLM.TX++;
#ifdef DEBUG_TNC
                printTime();
                Serial.println("TX->RF: " + String(txQueue[i].Info));
                // lastPkg = true;
                // lastPkgRaw = "TX->RF: " + String(txQueue[i].Info);
#endif
                return true;
            }
        }
    }
    return false;
}

uint8_t *packetData;

void aprs_msg_callback(struct AX25Msg *msg) {
    AX25Msg pkg;
    memcpy(&pkg, msg, sizeof(AX25Msg));
#ifdef USE_KISS
    kiss_messageCallback(ctx);
#endif
    PacketBuffer.push(&pkg);
//TODO processPacket();
}

uint8_t gwRaw[PKGLISTSIZE][66];
uint8_t gwRawSize[PKGLISTSIZE];
int gwRaw_count = 0, gwRaw_idx_rd = 0, gwRaw_idx_rw = 0;

void pushGwRaw(uint8_t *raw, uint8_t size) {
    if (gwRaw_count > PKGLISTSIZE) return;
    if (++gwRaw_idx_rw >= PKGLISTSIZE) gwRaw_idx_rw = 0;
    if (size > 65) size = 65;
    memcpy(&gwRaw[gwRaw_idx_rw][0], raw, size);
    gwRawSize[gwRaw_idx_rw] = size;
    gwRaw_count++;
}

uint8_t popGwRaw(uint8_t *raw) {
    uint8_t size = 0;
    if (gwRaw_count <= 0) return 0;
    if (++gwRaw_idx_rd >= PKGLISTSIZE) gwRaw_idx_rd = 0;
    size = gwRawSize[gwRaw_idx_rd];
    memcpy(raw, &gwRaw[gwRaw_idx_rd][0], size);
    if (gwRaw_count > 0) gwRaw_count--;
    return size;
}

WiFiClient aprsClient;

boolean APRSConnect() {
    // Serial.println("Connect TCP Server");
    String login = "";
    int cnt = 0;
    uint8_t con = aprsClient.connected();
    // Serial.println(con);
    if (con <= 0) {
        if (!aprsClient.connect(config.aprs_host, config.aprs_port)) {
            // Serial.print(".");
            delay(100);
            cnt++;
            if (cnt > 50) return false;
        }
        //
        if (config.aprs_ssid == 0) {
            login = "user " + String(config.aprs_mycall) + " pass " +
                    String(config.aprs_passcode) + " vers APRS-ESP V" +
                    String(VERSION) + " filter " + String(config.aprs_filter);
        } else {
            login = "user " + String(config.aprs_mycall) + "-" +
                    String(config.aprs_ssid) + " pass " +
                    String(config.aprs_passcode) + " vers APRS-ESP V" +
                    String(VERSION) + " filter " + String(config.aprs_filter);
        }
        aprsClient.println(login);
        // Serial.println(login);
        // Serial.println("Success");
        delay(500);
    }

    return true;
}

long oledSleepTimeout = 0;
bool showDisp = false;
uint8_t curTab = 0;

void setup()
{
    pinMode(BOOT_PIN, INPUT_PULLUP);  // BOOT Button

    // Set up serial port
    Serial.begin(SERIAL_DEBUG_BAUD);  // debug
    Serial.setRxBufferSize(256);

#if defined(USE_TNC)
    SerialTNC.begin(SERIAL_TNC_BAUD, SERIAL_8N1, SERIAL_TNC_RXPIN,
                    SERIAL_TNC_TXPIN);
    SerialTNC.setRxBufferSize(500);
#endif

#if defined(USE_GPS)
    SerialGPS.begin(SERIAL_GPS_BAUD, SERIAL_8N1, SERIAL_GPS_RXPIN,
                    SERIAL_GPS_TXPIN);
    SerialGPS.setRxBufferSize(500);
#endif

#ifndef USE_ROTARY
    pinMode(PIN_ROT_BTN, INPUT_PULLUP);
#endif

    Serial.println();
    Serial.println("Start APRS-ESP V" + String(VERSION));
    Serial.println("Press and hold BOOT button for 3 sec to Factory Default config");

#ifdef USE_SCREEN
    OledStartup();
#endif

    LoadConfig();

#ifdef USE_RF
    // RF SHOULD BE Initialized or there is no reason to startup at all
    while (!RF_Init(true)) {};
#endif

    // if SmartBeaconing - delay processing GPS data
    if (config.aprs_beacon == 0) {
        Serial.println("SmartBeaconing, delayed GPS processing");
        gpsUpdTMO = 30000;  // 30 sec
    }

#ifdef NEW_OLED
    if (config.oled_enable == true) {
        display.clearDisplay();
        display.setTextSize(1);
        display.display();
    }
#endif

    showDisp = true;
    curTab = 3;
#ifdef NEW_OLED
    oledSleepTimeout = millis() + (config.oled_timeout * 1000);
#else
    oledSleepTimeout = millis() + (60 * 1000);
#endif
 

    // enableLoopWDT();
    // enableCore0WDT();
    enableCore1WDT();

    // Task 1
    xTaskCreatePinnedToCore(taskAPRS,   /* Function to implement the task */
                            "taskAPRS", /* Name of the task */
                            8192,       /* Stack size in words */
                            NULL,       /* Task input parameter */
                            1,          /* Priority of the task */
                            &taskAPRSHandle, /* Task handle. */
                            0); /* Core where the task should run */

    // Task 2
    xTaskCreatePinnedToCore(taskNetwork,   /* Function to implement the task */
                            "taskNetwork", /* Name of the task */
                            16384,         /* Stack size in words */
                            NULL,          /* Task input parameter */
                            1,             /* Priority of the task */
                            &taskNetworkHandle, /* Task handle. */
                            1); /* Core where the task should run */
}

int pkgCount = 0;

String send_gps_location() {
    String tnc2Raw = "";
    
    float _lat;
    float _lon;

    //if (/*age != (uint32_t)ULONG_MAX &&*/ gps.location.isValid() /*|| gotGpsFix*/) {
    if (gps.location.isValid()) {
        _lat = lat / 1000000.0;
        _lon = lon / 1000000.0;
        distance = 0;   // Reset counted distance
        Serial.println("GPS Fix");
    } else {
        _lat = config.gps_lat;
        _lon = config.gps_lon;
        Serial.println("No GPS Fix");

        // Reset counted distance
        distance = 0;
    }

    int lat_dd, lat_mm, lat_ss, lon_dd, lon_mm, lon_ss;
    char strtmp[300], loc[30];

    memset(strtmp, 0, 300);
    DD_DDDDDtoDDMMSS(_lat, &lat_dd, &lat_mm, &lat_ss);
    DD_DDDDDtoDDMMSS(_lon, &lon_dd, &lon_mm, &lon_ss);

    sprintf(loc, "=%02d%02d.%02dN%c%03d%02d.%02dE%c", 
            lat_dd, lat_mm, lat_ss, config.aprs_table, lon_dd, lon_mm, lon_ss, config.aprs_symbol);

    if (config.aprs_ssid == 0)
        sprintf(strtmp, "%s>APZ32E", config.aprs_mycall);
    else
        sprintf(strtmp, "%s-%d>APZ32E", config.aprs_mycall, config.aprs_ssid);

    tnc2Raw = String(strtmp);

    if (config.aprs_path[0] != 0) {
        tnc2Raw += ",";
        tnc2Raw += String(config.aprs_path);
    }

    tnc2Raw += ":";
    tnc2Raw += String(loc);
    tnc2Raw += String(config.aprs_comment);

    return tnc2Raw;
}

int packet2Raw(String &tnc2, AX25Msg &Packet) {
    if (Packet.len < 5) return 0;

    tnc2 = String(Packet.src.call);

    if (Packet.src.ssid > 0) {
        tnc2 += String(F("-"));
        tnc2 += String(Packet.src.ssid);
    }

    tnc2 += String(F(">"));
    tnc2 += String(Packet.dst.call);

    if (Packet.dst.ssid > 0) {
        tnc2 += String(F("-"));
        tnc2 += String(Packet.dst.ssid);
    }

    for (int i = 0; i < Packet.rpt_count; i++) {
        tnc2 += String(",");
        tnc2 += String(Packet.rpt_list[i].call);
        if (Packet.rpt_list[i].ssid > 0) {
            tnc2 += String("-");
            tnc2 += String(Packet.rpt_list[i].ssid);
        }
        if (Packet.rpt_flags & (1 << i)) tnc2 += "*";
    }

    tnc2 += String(F(":"));
    tnc2 += String((const char *)Packet.info);
    tnc2 += String("\n");

    // #ifdef DEBUG_TNC
    //     Serial.printf("[%d] ", ++pkgTNC_count);
    //     Serial.print(tnc2);
    // #endif

    return tnc2.length();
}

void printPeriodicDebug() {
    printTime();
    Serial.print("lat: ");
    Serial.print(lat);
    Serial.print(" lon: ");
    Serial.print(lon);
    Serial.print(" age: ");
    Serial.print(age);
    Serial.print(gps.location.isValid() ? ", valid" : ", invalid");
    Serial.print(gps.location.isUpdated() ? ", updated" : ", not updated");
    Serial.print(", dist: ");
    Serial.println(distance);
}

bool gpsUnlock = false;

void updateScreenAndGps(bool force) {
    // GpsUpdate();

    // 1/sec
    if ((millis() - scrUpdTMO > 1000) || force) {
        OledUpdate();
        printPeriodicDebug();
        scrUpdTMO = millis();
    }

    // If startup dealy not expired
    if (!gpsUnlock) {
        if ((long)millis() - gpsUpdTMO < 0) {
            return;
        } else {
            Serial.println("GPS Unlocked");
            gpsUnlock = true;
        }
    }
    // 1/sec
    if (millis() - gpsUpdTMO > 1000) {
        GpsUpdate();
        heuristicDistanceChanged();
        gpsUpdTMO = millis();
    }
}

long sendTimer = 0;
bool AFSKInitAct = false;
int btn_count = 0;
long timeCheck = 0;
int timeHalfSec = 0;

void loop()
{
    vTaskDelay(5 / portTICK_PERIOD_MS);  // 5 ms // remove?
    if (millis() > timeCheck) {
        timeCheck = millis() + 10000;
        if (ESP.getFreeHeap() < 60000) esp_restart();
        // Serial.println(String(ESP.getFreeHeap()));
    }
#ifdef USE_RF
#ifdef DEBUG_RF
#ifdef USE_SA828
    if (SerialRF.available()) {
        Serial.print(SerialRF.readString());
    }
#endif
#endif
#endif
    if (AFSKInitAct == true) {
#ifdef USE_RF
        AFSK_Poll(true, config.rf_power, POWER_PIN);
#else
        AFSK_Poll(false, LOW);
#endif
#ifdef USE_GPS
        // if (SerialGPS.available()) {
        //     String gpsData = SerialGPS.readStringUntil('\n');
        //     Serial.println(gpsData);
        // }
#endif
    }

    bool update_screen = RotaryProcess();

    // if (send_aprs_update) {
    //     update_screen |= true;
    //     // send_aprs_update = false;    // moved to APRS task
    // }
    // if (update_screen) OledUpdate();

#ifndef NEW_OLED
    updateScreenAndGps(update_screen);
#endif

    // Popup Display
#ifdef NEW_OLED
    if (config.oled_enable == true) {
#else
    if (1) {
#endif
        if (dispBuffer.getCount() > 0) {
#ifdef NEW_OLED
            if (millis() > timeHalfSec) {
#else
            if (1) {
#endif
                char tnc2[300];
                dispBuffer.pop(&tnc2);
                dispWindow(String(tnc2), 0, false);
            }
        } else {
            // Sleep display
            if (millis() > timeHalfSec) {
                if (timeHalfSec > 0) {
                    timeHalfSec = 0;
#ifdef NEW_OLED
                    oledSleepTimeout = millis() + (config.oled_timeout * 1000);
#else
                    oledSleepTimeout = millis() + (60 * 1000);
#endif
                } else {
                    if (millis() > oledSleepTimeout && oledSleepTimeout > 0) {
                        oledSleepTimeout = 0;
#ifdef NEW_OLED
                        display.clearDisplay();
                        display.display();
#endif
                    }
                }
            }
        }

        if (showDisp) {
            showDisp = false;
            timeHalfSec = 0;
#ifdef NEW_OLED
            oledSleepTimeout = millis() + (config.oled_timeout * 1000);
#else
            oledSleepTimeout = millis() + (60 * 1000);
#endif
            switch (curTab) {
            case 0:
                statisticsDisp();
                break;
            case 1:
                pkgLastDisp();
                break;
            case 2:
                pkgCountDisp();
                break;
            case 3:
                systemDisp();
                break;
            }
        }
    }

    uint8_t bootPin2 = HIGH;
#ifndef USE_ROTARY
    bootPin2 = digitalRead(PIN_ROT_BTN);
#endif

    if (digitalRead(BOOT_PIN) == LOW || bootPin2 == LOW) {
        btn_count++;
        if (btn_count > 1000)  // Push BOOT 10sec
        {
            digitalWrite(RX_LED_PIN, HIGH);
            digitalWrite(TX_LED_PIN, HIGH);
        }
    } else {
        if (btn_count > 0) {
            // Serial.printf("btn_count=%dms\n", btn_count * 10);
            if (btn_count > 1000)  // Push BOOT 10sec to Factory Default
            {
                DefaultConfig();
                Serial.println("SYSTEM REBOOT NOW!");
                esp_restart();
            } 
            else if (btn_count > 10)  // Push BOOT >100mS to PTT Fix location
            {
                if (config.tnc) {
                    String tnc2Raw = send_gps_location();
                    pkgTxUpdate(tnc2Raw.c_str(), 0);
                    // APRS_sendTNC2Pkt(tnc2Raw); // Send packet to RF
#ifdef DEBUG_TNC
                    Serial.println("Manual TX: " + tnc2Raw);
#endif
                }
            }
            btn_count = 0;
        }
    }
}

String sendIsAckMsg(String toCallSign, int msgId) {
    char str[300];
    char call[11];
    int i;
    memset(&call[0], 0, 11);
    sprintf(call, "%s-%d", config.aprs_mycall, config.aprs_ssid);
    strcpy(&call[0], toCallSign.c_str());
    i = strlen(call);
    for (; i < 9; i++)
        call[i] = 0x20;
    memset(&str[0], 0, 300);

    sprintf(str, "%s-%d>APZ32E%s::%s:ack%d", config.aprs_mycall, config.aprs_ssid, VERSION, call, msgId);
    //	client.println(str);
    return String(str);
}

void sendIsPkg(char *raw) {
    char str[300];
    sprintf(str, "%s-%d>APZ32E%s:%s", config.aprs_mycall, config.aprs_ssid, VERSION, raw);
    // client.println(str);
    String tnc2Raw = String(str);
    if (aprsClient.connected())
        aprsClient.println(tnc2Raw); // Send packet to Inet
    if (config.tnc && config.tnc_digi)
        pkgTxUpdate(str, 0);
}

void sendIsPkgMsg(char *raw) {
    char str[300];
    char call[11];
    int i;

    memset(&call[0], 0, 11);

    if (config.aprs_ssid == 0)
        sprintf(call, "%s", config.aprs_mycall);
    else
        sprintf(call, "%s-%d", config.aprs_mycall, config.aprs_ssid);

    i = strlen(call);

    for (; i < 9; i++) call[i] = 0x20;

    if (config.aprs_ssid == 0)
        sprintf(str, "%s>APZ32E::%s:%s", config.aprs_mycall, call, raw);
    else
        sprintf(str, "%s-%d>APZ32E::%s:%s", config.aprs_mycall, config.aprs_ssid, call, raw);

    String tnc2Raw = String(str);
    
    if (aprsClient.connected()) {
        aprsClient.println(tnc2Raw);  // Send packet to Inet
    }
    
    if (config.tnc && config.tnc_digi) {
        pkgTxUpdate(str, 0);
    }
    
    // APRS_sendTNC2Pkt(tnc2Raw); // Send packet to RF
}

long timeSlot;

void taskAPRS(void *pvParameters) {
    //	long start, stop;
    // char *raw;
    // char *str;
    Serial.println("Task <APRS> started");
    PacketBuffer.clean();

    APRS_init();
    APRS_setCallsign(config.aprs_mycall, config.aprs_ssid);
    APRS_setPath1(config.aprs_path, 1);
    APRS_setPreamble(APRS_PREAMBLE);
    APRS_setTail(APRS_TAIL);

#ifdef USE_KISS
    kiss_init(&AX25, &modem, &Serial, 0);
#endif

    sendTimer = millis() - (config.aprs_beacon * 60 * 1000) + 30000;
    igateTLM.TeleTimeout = millis() + 60000;  // 1Min
    AFSKInitAct = true;
    timeSlot = millis();
    
    for (;;) {
        long now = millis();
        // wdtSensorTimer = now;
        time_t timeStamp;
        time(&timeStamp);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        // serviceHandle();
        if (now > (timeSlot + 10)) {
            if (!digitalRead(RX_LED_PIN)) {  // RX State Fail
                if (pkgTxSend()) {
                    timeSlot = millis() + config.tx_timeslot;  // Tx Time Slot = 5sec.
                } else {
                    timeSlot = millis();
                }
            } else {
                timeSlot = millis() + 500;
            }
        }

#ifdef USE_RF
// if(digitalRead(SQL_PIN)==HIGH){
// 	delay(10);
// 	if(digitalRead(SQL_PIN)==LOW){
// 		while(SerialRF.available()) SerialRF.read();
// 		SerialRF.println("RSSI?");
// 		delay(100);
// 		String ret=SerialRF.readString();
// 		Serial.println(ret);
// 		if(ret.indexOf("RSSI=")>=0){
// 			String sig=getValue(ret,'=',2);
// 			Serial.printf("SIGNAL %s\n",sig.c_str());
// 		}
// 	}
// }
#endif

        bool sendByTime = false;
        if (config.aprs_beacon > 0) {   // it interval configured
            sendByTime = (now > (sendTimer + (config.aprs_beacon * 60 * 1000)));
        }
        bool sendByFlag = send_aprs_update;

        send_aprs_update = false;

        if (sendByTime || sendByFlag) {
            sendTimer = now;
            
            Serial.print("Send by ");
            if (sendByTime) Serial.println("Time");
            if (sendByFlag) Serial.println("Flag");

            if (digiCount > 0) digiCount--;
#ifdef USE_RF
            RF_Check();
#endif
            if (AFSKInitAct == true) {
                if (config.tnc) {
                    String tnc2Raw = send_gps_location();
                    if (aprsClient.connected()) {
                        aprsClient.println(tnc2Raw);  // Send packet to Inet
                    }
                    pkgTxUpdate(tnc2Raw.c_str(), 0);
                    // APRS_sendTNC2Pkt(tnc2Raw);       // Send packet to RF
#ifdef DEBUG_TNC
                    // Serial.println("TX: " + tnc2Raw);
#endif
                }
            }
            // send_gps_location();
            //  APRS_setCallsign(config.aprs_mycall, config.aprs_ssid);
            //  	APRS_setPath1("WIDE1", 1);
            //  	APRS_setPreamble(APRS_PREAMBLE);
            //  	APRS_setTail(APRS_TAIL);
            // APRS_sendTNC2Pkt("HS5TQA-6>APZ32E,TRACE2-2:=1343.76N/10026.06E&ESP32
            // APRS Internet Gateway");
        }

        if (config.tnc_telemetry) {
            if (igateTLM.TeleTimeout < millis()) {
                igateTLM.TeleTimeout =
                    millis() + TNC_TELEMETRY_PERIOD;  // 10Min
                if ((igateTLM.Sequence % 6) == 0) {
                    sendIsPkgMsg((char *)&PARM[0]);
                    sendIsPkgMsg((char *)&UNIT[0]);
                    sendIsPkgMsg((char *)&EQNS[0]);
                }
                char rawTlm[100];
                if (config.aprs_ssid == 0) {
                    sprintf(rawTlm, "%s>APZ32E:T#%03d,%d,%d,%d,%d,%d,00000000",
                            config.aprs_mycall, igateTLM.Sequence,
                            igateTLM.RF2INET, igateTLM.INET2RF, igateTLM.RX,
                            igateTLM.TX, igateTLM.DROP);
                } else {
                    sprintf(
                        rawTlm, "%s-%d>APZ32E:T#%03d,%d,%d,%d,%d,%d,00000000",
                        config.aprs_mycall, config.aprs_ssid, igateTLM.Sequence,
                        igateTLM.RF2INET, igateTLM.INET2RF, igateTLM.RX,
                        igateTLM.TX, igateTLM.DROP);
                }

                if (aprsClient.connected()) {
                    aprsClient.println(String(rawTlm));  // Send packet to Inet
                }
                if (config.tnc && config.tnc_digi) {
                    pkgTxUpdate(rawTlm, 0);
                }
                // APRS_sendTNC2Pkt(String(rawTlm)); // Send packet to RF
                igateTLM.Sequence++;
                if (igateTLM.Sequence > 999) igateTLM.Sequence = 0;
                igateTLM.DROP = 0;
                igateTLM.INET2RF = 0;
                igateTLM.RF2INET = 0;
                igateTLM.RX = 0;
                igateTLM.TX = 0;
                // client.println(raw);
            }
        }

        // IGate RF->INET
        if (config.tnc) {
            if (PacketBuffer.getCount() > 0) {
                String tnc2;
                PacketBuffer.pop(&incomingPacket);
//TODO          processPacket();
                // igateProcess(incomingPacket);
                packet2Raw(tnc2, incomingPacket);
#ifdef DEBUG_TNC                
                Serial.println("RX->RF: " + tnc2);
#endif
#ifdef NEW_OLED
                if (config.oled_enable == true){
#else
                if (1) {
#endif
                    // if (config.dispTNC == true)
                    dispBuffer.push(tnc2.c_str());
                }

                // IGate Process
                if (config.rf2inet && aprsClient.connected()) {
                    int ret = igateProcess(incomingPacket);
                    if (ret == 0) {
                        status.dropCount++;
                        igateTLM.DROP++;
                    } else {
                        status.rf2inet++;
                        igateTLM.RF2INET++;
                        // igateTLM.TX++;
#ifdef DEBUG
                        printTime();
                        Serial.print("RF->INET: ");
                        Serial.println(tnc2);
#endif
                        char call[11];
                        if (incomingPacket.src.ssid > 0) {
                            sprintf(call, "%s-%d", incomingPacket.src.call, incomingPacket.src.ssid);
                        } else {
                            sprintf(call, "%s", incomingPacket.src.call);
                        }
                        
                        uint8_t type = pkgType((char *)incomingPacket.info);
                        pkgListUpdate(call, type);
                    }
                }

                // Digi Repeater Process
                if (config.tnc_digi) {
                    int dlyFlag = digiProcess(incomingPacket);
                    if (dlyFlag > 0) {
                        int digiDelay;
                        status.digiCount++;
                        igateTLM.RX++;
                        if (dlyFlag == 1) {
                            digiDelay = 0;
                        } else {
                            if (config.digi_delay == 0) {  // Auto mode
                                if (digiCount > 20)
                                    digiDelay = random(5000);
                                else if (digiCount > 10)
                                    digiDelay = random(3000);
                                else if (digiCount > 0)
                                    digiDelay = random(1500);
                                else
                                    digiDelay = random(500);
                            } else {
                                digiDelay = random(config.digi_delay);
                            }
                        }
                        String digiPkg;
                        packet2Raw(digiPkg, incomingPacket);
                        pkgTxUpdate(digiPkg.c_str(), digiDelay);
                    }
                    else
                    {
                        igateTLM.DROP++;
                    }
                }

                lastPkg = true;
                lastPkgRaw = tnc2;
                // ESP_BT.println(tnc2);
                status.allCount++;
            }
        }
    }
}

long wifiTTL = 0;

void taskNetwork(void *pvParameters) {
    int c = 0;
    Serial.println("Task <Network> started");

    if (config.wifi_mode == WIFI_AP_STA_FIX || config.wifi_mode == WIFI_AP_FIX) {  // AP=false
        // WiFi.mode(config.wifi_mode);
        if (config.wifi_mode == WIFI_AP_STA_FIX) {
            WiFi.mode(WIFI_AP_STA);
        } else if (config.wifi_mode == WIFI_AP_FIX) {
            WiFi.mode(WIFI_AP);
        }
        // Configure Wi-Fi as an access point
        WiFi.softAP(config.wifi_ap_ssid, config.wifi_ap_pass);  // Start HOTspot removing password
        // will disable security
        WiFi.softAPConfig(local_IP, gateway, subnet);
        Serial.print("Access point running. IP address: ");
        Serial.print(WiFi.softAPIP());
        Serial.println("");
    } else if (config.wifi_mode == WIFI_STA_FIX) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        Serial.println(F("WiFi Station Only mode"));
    } else {
        WiFi.mode(WIFI_OFF);
        WiFi.disconnect(true);
        delay(100);
        // Serial.println(F("WiFi OFF. BT only mode"));
        Serial.println(F("WiFi OFF All mode"));
        // SerialBT.begin("ESP32TNC");
    }

    webService();
    pingTimeout = millis() + 10000;

    for (;;) {
        // wdtNetworkTimer = millis();
        vTaskDelay(5 / portTICK_PERIOD_MS);
        serviceHandle();

        if (config.wifi_mode == WIFI_AP_STA_FIX || config.wifi_mode == WIFI_STA_FIX) {
            if (WiFi.status() != WL_CONNECTED) {
                unsigned long int tw = millis();
                if (tw > wifiTTL) {
#ifndef I2S_INTERNAL
                    AFSK_TimerEnable(false);
#endif
                    wifiTTL = tw + 60000;
                    Serial.println("WiFi connecting...");
                    // udp.endPacket();
                    WiFi.disconnect();
                    WiFi.setTxPower((wifi_power_t)config.wifi_power);
                    WiFi.setHostname("APRS_ESP");
                    WiFi.begin(config.wifi_ssid, config.wifi_pass);
                    // Wait up to 1 minute for connection...
                    for (c = 0; (c < 30) && (WiFi.status() != WL_CONNECTED); c++) {
                        // Serial.write('.');
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        // for (t = millis(); (millis() - t) < 1000; refresh());
                    }
                    if (c >= 30) {  // If it didn't connect within 1 min
                        Serial.println("Failed. Will retry...");
                        WiFi.disconnect();
                        // WiFi.mode(WIFI_OFF);
                        delay(3000);
                        // WiFi.mode(WIFI_STA);
                        WiFi.reconnect();
                        continue;
                    }

                    Serial.println("WiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());

                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    NTP_Timeout = millis() + 5000;
// Serial.println("Contacting Time Server");
// configTime(3600 * timeZone, 0, "aprs.dprns.com", "1.pool.ntp.org");
// vTaskDelay(3000 / portTICK_PERIOD_MS);
#ifndef I2S_INTERNAL
                    AFSK_TimerEnable(true);
#endif
                }
            } else {
                if (millis() > NTP_Timeout) {
                    NTP_Timeout = millis() + 86400000;
                    // Serial.println("Config NTP");
                    // setSyncProvider(getNtpTime);
                    Serial.println("Setting up NTP");
                    configTime(3600 * config.timeZone, 0, "203.150.19.26", "110.170.126.101", "77.68.122.252");
                    vTaskDelay(3000 / portTICK_PERIOD_MS);
                    time_t systemTime;
                    time(&systemTime);
                    setTime(systemTime);
                    if (systemUptime == 0) {
                        systemUptime = now();
                    }
                    pingTimeout = millis() + 2000;
                }

                if (config.aprs) {
                    if (aprsClient.connected() == false) {
                        APRSConnect();
                    } else {
                        if (aprsClient.available()) {
                            pingTimeout = millis() + 300000; // Reset ping
                            String line = aprsClient.readStringUntil('\n');  //read the value at Server answer sleep line by line
#ifdef DEBUG_IS
                            printTime();
                            Serial.print("APRS-IS: ");
                            Serial.println(line);
#endif
                            status.isCount++;

                            int start_val = line.indexOf(">", 0);  // find the first position of >
                            if (start_val > 3) {
#ifdef NEW_OLED
                                if (config.dispINET == true)
#else
                                if (1) {
#endif
                                    dispBuffer.push(line.c_str());
                                }

                                // raw = (char *)malloc(line.length() + 1);
                                String src_call = line.substring(0, start_val);
                                int msg_call_idx = line.indexOf("::");  // text only
                                String msg_call = "";
                                if (msg_call_idx > 0) {
                                    msg_call = line.substring(msg_call_idx + 2, msg_call_idx + 9);
                                }
#ifdef DEBUG_IS
                                Serial.printf("SRC_CALL: %s\r\n", src_call.c_str());
                                if (msg_call_idx > 0) {
                                    Serial.printf("MSG_CALL: %s\r\n", msg_call.c_str());
                                }
#endif
                                status.allCount++;
                                // igateTLM.RX++;

                                // Is it not Telemetry?
                                if (line.indexOf(":T#") < 0
                                    && line.indexOf("PARM.") < 0
                                    && line.indexOf("UNIT.") < 0
                                    && line.indexOf("EQNS.") < 0
                                    && line.indexOf("BITS.") < 0)
                                {
                                    // Is INET2RF configured and MSG_CALL present
                                    if (config.tnc && config.inet2rf && (msg_call != "")) {
                                        // Is adresse is in owner callsigns group?
                                        if (msg_call.startsWith(config.aprs_mycall)) {
                                            Serial.println("MSG to Owner group");
                                            pkgTxUpdate(line.c_str(), 0);
                                            status.inet2rf++;
                                            igateTLM.INET2RF++;
                                            printTime();
#ifdef DEBUG
                                            Serial.print("INET->RF " + line);
#endif
                                        } else {
                                            // Is it a message for last heard stations?
                                            for (int i = 0; i < PKGLISTSIZE; i++) {
                                                if (pkgList[i].time > 0) {
                                                    if (strcmp(pkgList[i].calsign, msg_call.c_str()) == 0) {
                                                        Serial.println("MSG to last heard");
                                                        pkgTxUpdate(line.c_str(), 0);
                                                        status.inet2rf++;
                                                        igateTLM.INET2RF++;
                                                        printTime();
                                                        Serial.println("INET->RF " + line);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else {
                                    // Telemetry found
                                    igateTLM.DROP++;
                                    Serial.print("INET Message TELEMETRY from ");
                                    Serial.println(src_call);
                                }
                            }
                        }
                    }
                }

                if (millis() > pingTimeout) {
                    pingTimeout = millis() + 300000;
                    Serial.println("Ping GW to " + WiFi.gatewayIP().toString());
                    if (ping_start(WiFi.gatewayIP(), 3, 0, 0, 5) == true) {
                        Serial.println("Ping GW OK");
                    } else {
                        Serial.println("Ping GW Fail");
                        WiFi.disconnect();
                        wifiTTL = 0;
                    }
                }
            }
        }
    }
}

void line_angle(signed int startx, signed int starty, unsigned int length, unsigned int angle, unsigned int color) {
#ifdef NEW_OLED
    display.drawLine(startx, starty, (startx + length * cosf(angle * 0.017453292519)), (starty + length * sinf(angle * 0.017453292519)), color);
#endif
}

int xSpiGlcdSelFontHeight = 8;
int xSpiGlcdSelFontWidth = 5;

void compass_label(signed int startx, signed int starty, unsigned int length, double angle, unsigned int color) {
#ifdef NEW_OLED
    double angleNew;
    // ushort Color[2];
    uint8_t x_N, y_N, x_S, y_S;
    int x[4], y[4], i;
    int xOffset, yOffset;
    yOffset = (xSpiGlcdSelFontHeight / 2);
    xOffset = (xSpiGlcdSelFontWidth / 2);
    // GLCD_WindowMax();
    angle += 270.0F;
    angleNew = angle;
    for (i = 0; i < 4; i++) {
        if (angleNew > 360.0F)
            angleNew -= 360.0F;
        x[i] = startx + (length * cosf(angleNew * 0.017453292519));
        y[i] = starty + (length * sinf(angleNew * 0.017453292519));
        x[i] -= xOffset;
        y[i] -= yOffset;
        angleNew += 90.0F;
    }
    angleNew = angle + 45.0F;
    for (i = 0; i < 4; i++) {
        if (angleNew > 360.0F)
            angleNew -= 360.0F;
        x_S = startx + ((length - 3) * cosf(angleNew * 0.017453292519));
        y_S = starty + ((length - 3) * sinf(angleNew * 0.017453292519));
        x_N = startx + ((length + 3) * cosf(angleNew * 0.017453292519));
        y_N = starty + ((length + 3) * sinf(angleNew * 0.017453292519));
        angleNew += 90.0F;
        display.drawLine(x_S, y_S, x_N, y_N, color);
    }
    display.drawCircle(startx, starty, length, color);
    display.setFont();
    display.drawChar((uint8_t)x[0], (uint8_t)y[0], 'N', WHITE, BLACK, 1);
    display.drawChar((uint8_t)x[1], (uint8_t)y[1], 'E', WHITE, BLACK, 1);
    display.drawChar((uint8_t)x[2], (uint8_t)y[2], 'S', WHITE, BLACK, 1);
    display.drawChar((uint8_t)x[3], (uint8_t)y[3], 'W', WHITE, BLACK, 1);
#endif
}

void compass_arrow(signed int startx, signed int starty, unsigned int length, double angle, unsigned int color) {
#ifdef NEW_OLED
    double angle1, angle2;
    int xdst, ydst, x1sta, y1sta, x2sta, y2sta;
    int length2 = length / 2;
    angle += 270.0F;
    if (angle > 360.0F)
        angle -= 360.0F;
    xdst = startx + length * cosf(angle * 0.017453292519);
    ydst = starty + length * sinf(angle * 0.017453292519);
    angle1 = angle + 135.0F;
    if (angle1 > 360.0F)
        angle1 -= 360.0F;
    angle2 = angle + 225.0F;
    if (angle2 > 360.0F)
        angle2 -= 360.0F;
    x1sta = startx + length2 * cosf(angle1 * 0.017453292519);
    y1sta = starty + length2 * sinf(angle1 * 0.017453292519);
    x2sta = startx + length2 * cosf(angle2 * 0.017453292519);
    y2sta = starty + length2 * sinf(angle2 * 0.017453292519);

    display.drawLine(startx, starty, xdst, ydst, color);
    display.drawLine(xdst, ydst, x1sta, y1sta, color);
    display.drawLine(x1sta, y1sta, startx, starty, color);
    display.drawLine(startx, starty, x2sta, y2sta, color);
    display.drawLine(x2sta, y2sta, xdst, ydst, color);
#endif
}

const char *directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
bool dispPush = 0;
unsigned long disp_delay = 0;

void dispWindow(String line, uint8_t mode, bool filter) {
#ifdef NEW_OLED
    uint16_t bgcolor, txtcolor;
    bool Monitor = false;
    char text[200];
    unsigned char x = 0;
    char itemname[10];
    int start_val = line.indexOf(">", 0); // หาตำแหน่งแรกของ >
    if (start_val > 3) {
        // Serial.println(line);
        String src_call = line.substring(0, start_val);
        memset(&aprs, 0, sizeof(pbuf_t));
        aprs.buf_len = 300;
        aprs.packet_len = line.length();
        line.toCharArray(&aprs.data[0], aprs.packet_len);
        int start_info = line.indexOf(":", 0);
        int end_ssid = line.indexOf(",", 0);
        int start_dst = line.indexOf(">", 2);
        int start_dstssid = line.indexOf("-", start_dst);
        if ((start_dstssid > start_dst) && (start_dstssid < start_dst + 10)) {
            aprs.dstcall_end_or_ssid = &aprs.data[start_dstssid];
        } else {
            aprs.dstcall_end_or_ssid = &aprs.data[end_ssid];
        }
        aprs.info_start = &aprs.data[start_info + 1];
        aprs.dstname = &aprs.data[start_dst + 1];
        aprs.dstname_len = end_ssid - start_dst;
        aprs.dstcall_end = &aprs.data[end_ssid];
        aprs.srccall_end = &aprs.data[start_dst];

        // Serial.println(aprs.info_start);
        // aprsParse.parse_aprs(&aprs);
        if (aprsParse.parse_aprs(&aprs)) {
            if (filter == true) {
                if (config.filterStatus && (aprs.packettype & T_STATUS)) {
                    Monitor = true;
                } else if (config.filterMessage && (aprs.packettype & T_MESSAGE)) {
                    Monitor = true;
                } else if (config.filterTelemetry && (aprs.packettype & T_TELEMETRY)) {
                    Monitor = true;
                } else if (config.filterWeather && (aprs.packettype & T_WX)) {
                    Monitor = true;
                }

                if (config.filterPosition && (aprs.packettype & T_POSITION)) {
                    double lat, lon;
                    if ((config.mygps == true) && gps.location.isValid()) {
                        lat = gps.location.lat();
                        lon = gps.location.lng();
                    } else {
                        lat = config.gps_lat;
                        lon = config.gps_lon;
                    }
                    double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);

                    if (config.filterDistant == 0) {
                        Monitor = true;
                    } else {
                        if (dist < config.filterDistant)
                            Monitor = true;
                        else
                            Monitor = false;
                    }
                }

                if (config.filterTracker && (aprs.packettype & T_POSITION)) {
                    if (aprs.flags & F_CSRSPD) {
                        double lat, lon;
                        if ((config.mygps == true) && gps.location.isValid()) {
                            lat = gps.location.lat();
                            lon = gps.location.lng();
                        } else {
                            lat = config.gps_lat;
                            lon = config.gps_lon;
                        }
                        double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                        if (config.filterDistant == 0) {
                            Monitor = true;
                        } else {
                            if (dist < config.filterDistant)
                                Monitor = true;
                            else
                                Monitor = false;
                        }
                    }
                }

                if (config.filterMove && (aprs.packettype & T_POSITION)) {
                    if (aprs.flags & F_CSRSPD) {
                        if (aprs.speed > 0) {
                            double lat, lon;
                            if ((config.mygps == true) && gps.location.isValid()) {
                                lat = gps.location.lat();
                                lon = gps.location.lng();
                            } else {
                                lat = config.gps_lat;
                                lon = config.gps_lon;
                            }
                            double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                            if (config.filterDistant == 0) {
                                Monitor = true;
                            } else {
                                if (dist < config.filterDistant)
                                    Monitor = true;
                                else
                                    Monitor = false;
                            }
                        }
                    }
                }
            } else {
                Monitor = true;
            }
        } else {
            return;
        }

        if (Monitor) {
            display.clearDisplay();
            if (dispPush) {
                disp_delay = 600 * 1000;
                display.drawRoundRect(0, 0, 128, 16, 5, WHITE);
            } else {
                disp_delay = config.dispDelay * 1000;
            }
            timeHalfSec = millis() + disp_delay;
            // display.fillRect(0, 0, 128, 16, WHITE);
            const uint8_t *ptrSymbol;
            uint8_t symIdx = aprs.symbol[1] - 0x21;
            if (symIdx > 95)
                symIdx = 0;
            if (aprs.symbol[0] == '/') {
                ptrSymbol = &Icon_TableA[symIdx][0];
            } else if (aprs.symbol[0] == '\\') {
                ptrSymbol = &Icon_TableB[symIdx][0];
            } else {
                if (aprs.symbol[0] < 'A' || aprs.symbol[0] > 'Z') {
                    aprs.symbol[0] = 'N';
                    aprs.symbol[1] = '&';
                    symIdx = 5; // &
                }
                ptrSymbol = &Icon_TableB[symIdx][0];
            }
            display.drawYBitmap(0, 0, ptrSymbol, 16, 16, WHITE);
            if (!(aprs.symbol[0] == '/' || aprs.symbol[0] == '\\')) {
                display.drawChar(5, 4, aprs.symbol[0], BLACK, WHITE, 1);
                display.drawChar(6, 5, aprs.symbol[0], BLACK, WHITE, 1);
            }
            display.setCursor(20, 7);
            display.setTextSize(1);
            display.setFont(&FreeSansBold9pt7b);

            if (aprs.srcname_len > 0) {
                memset(&itemname, 0, sizeof(itemname));
                memcpy(&itemname, aprs.srcname, aprs.srcname_len);
                Serial.println(itemname);
                display.print(itemname);
            } else {
                display.print(src_call);
            }

            display.setFont();
            display.setTextColor(WHITE);
            // if(selTab<10)
            // 	display.setCursor(121, 0);
            // else
            // 	display.setCursor(115, 0);
            // display.print(selTab);

            if (mode == 1) {
                display.drawRoundRect(0, 16, 128, 48, 5, WHITE);
                display.fillRoundRect(1, 17, 126, 10, 2, WHITE);
                display.setTextColor(BLACK);
                display.setCursor(40, 18);
                display.print("TNC2 RAW");

                display.setFont();
                display.setCursor(2, 30);
                display.setTextColor(WHITE);
                display.print(line);

                display.display();
                return;
            }

            if (aprs.packettype & T_TELEMETRY) {
                bool show = false;
                int idx = tlmList_Find((char *)src_call.c_str());
                if (idx < 0) {
                    idx = tlmListOld();
                    if (idx > -1)
                        memset(&Telemetry[idx], 0, sizeof(Telemetry_struct));
                }
                if (idx > -1) {
                    Telemetry[idx].time = now();
                    strcpy(Telemetry[idx].callsign, (char *)src_call.c_str());

                    // for (int i = 0; i < 3; i++) Telemetry[idx].UNIT[i][5] = 0;
                    if (aprs.flags & F_UNIT) {
                        memcpy(Telemetry[idx].UNIT, aprs.tlm_unit.val, sizeof(Telemetry[idx].UNIT));
                    } else if (aprs.flags & F_PARM) {
                        memcpy(Telemetry[idx].PARM, aprs.tlm_parm.val, sizeof(Telemetry[idx].PARM));
                    } else if (aprs.flags & F_EQNS) {
                        Telemetry[idx].EQNS_FLAG = true;
                        for (int i = 0; i < 15; i++)
                            Telemetry[idx].EQNS[i] = aprs.tlm_eqns.val[i];
                    } else if (aprs.flags & F_BITS) {
                        Telemetry[idx].BITS_FLAG = aprs.telemetry.bitsFlag;
                    } else if (aprs.flags & F_TLM) {
                        for (int i = 0; i < 5; i++)
                            Telemetry[idx].RAW[i] = aprs.telemetry.val[i];
                        Telemetry[idx].BITS = aprs.telemetry.bits;
                        show = true;
                    }

                    for (int i = 0; i < 4; i++) { // Cut length
                        if (strstr(Telemetry[idx].PARM[i], "RxTraffic") != 0)
                            sprintf(Telemetry[idx].PARM[i], "RX");
                        if (strstr(Telemetry[idx].PARM[i], "TxTraffic") != 0)
                            sprintf(Telemetry[idx].PARM[i], "TX");
                        if (strstr(Telemetry[idx].PARM[i], "RxDrop") != 0)
                            sprintf(Telemetry[idx].PARM[i], "DROP");
                        Telemetry[idx].PARM[i][6] = 0;
                        Telemetry[idx].UNIT[i][3] = 0;
                        for (int a = 0; a < 3; a++) {
                            if (Telemetry[idx].UNIT[i][a] == '/')
                                Telemetry[idx].UNIT[i][a] = 0;
                        }
                    }

                    for (int i = 0; i < 5; i++) {
                        if (Telemetry[idx].PARM[i][0] == 0) {
                            sprintf(Telemetry[idx].PARM[i], "CH%d", i + 1);
                        }
                    }
                }
                if (show || filter == false) {
                    display.drawRoundRect(0, 16, 128, 48, 5, WHITE);
                    display.fillRoundRect(1, 17, 126, 10, 2, WHITE);
                    display.setTextColor(BLACK);
                    display.setCursor(40, 18);
                    display.print("TELEMETRY");
                    display.setFont();
                    display.setTextColor(WHITE);
                    display.setCursor(2, 28);
                    display.print(Telemetry[idx].PARM[0]);
                    display.print(":");

                    if (Telemetry[idx].EQNS_FLAG == true) {
                        for (int i = 0; i < 5; i++) {
                            // a*v^2+b*v+c
                            Telemetry[idx].VAL[i] = (Telemetry[idx].EQNS[(i * 3) + 0] * pow(Telemetry[idx].RAW[i], 2));
                            Telemetry[idx].VAL[i] += Telemetry[idx].EQNS[(i * 3) + 1] * Telemetry[idx].RAW[i];
                            Telemetry[idx].VAL[i] += Telemetry[idx].EQNS[(i * 3) + 2];
                        }
                    } else {
                        for (int i = 0; i < 5; i++) {
                            // a*v^2+b*v+c
                            Telemetry[idx].VAL[i] = Telemetry[idx].RAW[i];
                        }
                    }

                    if (fmod(Telemetry[idx].VAL[0], 1) == 0)
                        display.print(Telemetry[idx].VAL[0], 0);
                    else
                        display.print(Telemetry[idx].VAL[0], 1);
                    display.print(Telemetry[idx].UNIT[0]);
                    display.setCursor(65, 28);
                    display.print(Telemetry[idx].PARM[1]);
                    display.print(":");
                    if (fmod(Telemetry[idx].VAL[1], 1) == 0)
                        display.print(Telemetry[idx].VAL[1], 0);
                    else
                        display.print(Telemetry[idx].VAL[1], 1);
                    display.print(Telemetry[idx].UNIT[1]);
                    display.setCursor(2, 37);
                    display.print(Telemetry[idx].PARM[2]);
                    display.print(":");
                    if (fmod(Telemetry[idx].VAL[2], 1) == 0)
                        display.print(Telemetry[idx].VAL[2], 0);
                    else
                        display.print(Telemetry[idx].VAL[2], 1);
                    display.print(Telemetry[idx].UNIT[2]);
                    display.setCursor(65, 37);
                    display.print(Telemetry[idx].PARM[3]);
                    display.print(":");
                    if (fmod(Telemetry[idx].VAL[3], 1) == 0)
                        display.print(Telemetry[idx].VAL[3], 0);
                    else
                        display.print(Telemetry[idx].VAL[3], 1);
                    display.print(Telemetry[idx].UNIT[3]);
                    display.setCursor(2, 46);
                    display.print(Telemetry[idx].PARM[4]);
                    display.print(":");
                    display.print(Telemetry[idx].VAL[4], 1);
                    display.print(Telemetry[idx].UNIT[4]);

                    display.setCursor(4, 55);
                    display.print("BIT");
                    uint8_t bit = Telemetry[idx].BITS;
                    for (int i = 0; i < 8; i++) {
                        if (bit & 0x80) {
                            display.fillCircle(30 + (i * 12), 58, 3, WHITE);
                        } else {
                            display.drawCircle(30 + (i * 12), 58, 3, WHITE);
                        }
                        bit <<= 1;
                    }
                    // display.print(Telemetry[idx].BITS, BIN);

                    // display.setFont();
                    // display.setCursor(2, 30);
                    // memset(&text[0], 0, sizeof(text));
                    // memcpy(&text[0], aprs.comment, aprs.comment_len);
                    // display.setTextColor(WHITE);
                    // display.print(aprs.comment);
                    display.display();
                }
                return;
            } else if (aprs.packettype & T_STATUS) {
                display.drawRoundRect(0, 16, 128, 48, 5, WHITE);
                display.fillRoundRect(1, 17, 126, 10, 2, WHITE);
                display.setTextColor(BLACK);
                display.setCursor(48, 18);
                display.print("STATUS");

                display.setFont();
                display.setCursor(2, 30);
                // memset(&text[0], 0, sizeof(text));
                // memcpy(&text[0], aprs.comment, aprs.comment_len);
                display.setTextColor(WHITE);
                display.print(aprs.comment);
                display.display();
                return;
            } else if (aprs.packettype & T_QUERY) {
                display.drawRoundRect(0, 16, 128, 48, 5, WHITE);
                display.fillRoundRect(1, 17, 126, 10, 2, WHITE);
                display.setTextColor(BLACK);
                display.setCursor(48, 18);
                display.print("?QUERY?");
                // memset(&text[0], 0, sizeof(text));
                // memcpy(&text[0], aprs.comment, aprs.comment_len);
                display.setFont();
                display.setTextColor(WHITE);
                display.setCursor(2, 30);
                display.print(aprs.comment);
                display.display();
                return;
            } else if (aprs.packettype & T_MESSAGE) {
                if (aprs.msg.is_ack == 1) {
                } else if (aprs.msg.is_rej == 1) {
                } else {
                    display.drawRoundRect(0, 16, 128, 48, 5, WHITE);
                    display.fillRoundRect(1, 17, 126, 10, 2, WHITE);
                    display.setTextColor(BLACK);
                    display.setCursor(48, 18);
                    display.print("MESSAGE");
                    display.setCursor(108, 18);
                    display.print("{");
                    strncpy(&text[0], aprs.msg.msgid, aprs.msg.msgid_len);
                    int msgid = atoi(text);
                    display.print(msgid, DEC);
                    display.print("}");
                    // memset(&text[0], 0, sizeof(text));
                    // memcpy(&text[0], aprs.comment, aprs.comment_len);
                    display.setFont();
                    display.setTextColor(WHITE);
                    display.setCursor(2, 30);
                    display.print("To: ");
                    strncpy(&text[0], aprs.dstname, aprs.dstname_len);
                    display.print(text);
                    String mycall;
                    if (config.aprs_ssid == 0)
                        mycall = config.aprs_mycall;
                    else
                        mycall = config.aprs_mycall + String("-") + String(config.aprs_ssid, DEC);
                    if (strcmp(mycall.c_str(), text) == 0) {
                        display.setCursor(2, 54);
                        display.print("ACK:");
                        display.println(msgid);
                        sendIsAckMsg(src_call, msgid);
                        // client.println(raw);
                        // SerialTNC.println("}" + raw);
                        // if (slot == 0) {
                        //	client.println(raw);
                        //}
                        // else {
                        //	SerialTNC.println("}" + raw);
                        //}
                    }
                    strncpy(&text[0], aprs.msg.body, aprs.msg.body_len);
                    display.setCursor(2, 40);
                    display.print("Msg: ");
                    display.println(text);

                    display.display();
                }
                return;
            }
            display.setFont();
            display.drawFastHLine(0, 16, 128, WHITE);
            display.drawFastVLine(48, 16, 48, WHITE);
            x = 8;

            if (aprs.srcname_len > 0) {
                x += 9;
                display.fillRoundRect(51, 16, 77, 9, 2, WHITE);
                display.setTextColor(BLACK);
                display.setCursor(53, x);
                display.print("By " + src_call);
                display.setTextColor(WHITE);
                // x += 9;
            }
            if (aprs.packettype & T_WX) {
                // Serial.println("WX Display");
                if (aprs.wx_report.flags & W_TEMP) {
                    display.setCursor(58, x += 10);
                    display.drawYBitmap(51, x, &Temperature_Symbol[0], 5, 8, WHITE);
                    display.printf("%.1fC", aprs.wx_report.temp);
                }
                if (aprs.wx_report.flags & W_HUM) {
                    display.setCursor(102, x);
                    display.drawYBitmap(95, x, &Humidity_Symbol[0], 5, 8, WHITE);
                    display.printf("%d%%", aprs.wx_report.humidity);
                }
                if (aprs.wx_report.flags & W_BAR) {
                    display.setCursor(58, x += 9);
                    display.drawYBitmap(51, x, &Pressure_Symbol[0], 5, 8, WHITE);
                    display.printf("%.1fhPa", aprs.wx_report.pressure);
                }
                if (aprs.wx_report.flags & W_R24H) {
                    // if (aprs.wx_report.rain_1h > 0) {
                    display.setCursor(58, x += 9);
                    display.drawYBitmap(51, x, &Rain_Symbol[0], 5, 8, WHITE);
                    display.printf("%.1fmm.", aprs.wx_report.rain_24h);
                    //}
                }
                if (aprs.wx_report.flags & W_PAR) {
                    // if (aprs.wx_report.luminosity > 10) {
                    display.setCursor(51, x += 9);
                    display.printf("%c", 0x0f);
                    display.setCursor(58, x);
                    display.printf("%dW/m", aprs.wx_report.luminosity);
                    if (aprs.wx_report.flags & W_UV) {
                        display.printf(" UV%d", aprs.wx_report.uv);
                    }
                    //}
                }
                if (aprs.wx_report.flags & W_WS) {
                    display.setCursor(58, x += 9);
                    display.drawYBitmap(51, x, &Wind_Symbol[0], 5, 8, WHITE);
                    // int dirIdx=map(aprs.wx_report.wind_dir, -180, 180, 0, 8); ((angle+22)/45)%8]
                    int dirIdx = ((aprs.wx_report.wind_dir + 22) / 45) % 8;
                    if (dirIdx > 8)
                        dirIdx = 8;
                    display.printf("%.1fkPh(%s)", aprs.wx_report.wind_speed, directions[dirIdx]);
                }
                // Serial.printf("%.1fkPh(%d)", aprs.wx_report.wind_speed, aprs.wx_report.wind_dir);
                if (aprs.flags & F_HASPOS) {
                    // Serial.println("POS Display");
                    double lat, lon;
                    if ((config.mygps == true) && gps.location.isValid()) {
                        lat = gps.location.lat();
                        lon = gps.location.lng();
                    } else {
                        lat = config.gps_lat;
                        lon = config.gps_lon;
                    }
                    double dtmp = aprsParse.direction(lon, lat, aprs.lng, aprs.lat);
                    double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                    // if (config.h_up == true) {
                    // 	//double course = gps.course.deg();
                    // 	double course = SB_HEADING;
                    // 	if (dtmp >= course) {
                    // 		dtmp -= course;
                    // 	}
                    // 	else {
                    // 		double diff = dtmp - course;
                    // 		dtmp = diff + 360.0F;
                    // 	}
                    // 	compass_label(25, 37, 15, course, WHITE);
                    // 	display.setCursor(0, 17);
                    // 	display.printf("H");
                    // }
                    // else {
                    compass_label(25, 37, 15, 0.0F, WHITE);
                    //}
                    // compass_label(25, 37, 15, 0.0F, WHITE);
                    compass_arrow(25, 37, 12, dtmp, WHITE);
                    display.drawFastHLine(1, 63, 45, WHITE);
                    display.drawFastVLine(1, 58, 5, WHITE);
                    display.drawFastVLine(46, 58, 5, WHITE);
                    display.setCursor(4, 55);
                    if (dist > 999)
                        display.printf("%.fKm", dist);
                    else
                        display.printf("%.1fKm", dist);
                } else {
                    display.setCursor(20, 30);
                    display.printf("NO\nPOSITION");
                }
            } else if (aprs.flags & F_HASPOS) {
                // display.setCursor(50, x += 10);
                // display.printf("LAT %.5f\n", aprs.lat);
                // display.setCursor(51, x+=9);
                // display.printf("LNG %.4f\n", aprs.lng);
                String str;
                int l = 0;
                display.setCursor(50, x += 10);
                display.print("LAT:");
                str = String(aprs.lat, 5);
                l = str.length() * 6;
                display.setCursor(128 - l, x);
                display.print(str);

                display.setCursor(50, x += 9);
                display.print("LON:");
                str = String(aprs.lng, 5);
                l = str.length() * 6;
                display.setCursor(128 - l, x);
                display.print(str);

                double lat, lon;
                if ((config.mygps == true) && gps.location.isValid()) {
                    lat = gps.location.lat();
                    lon = gps.location.lng();
                } else {
                    lat = config.gps_lat;
                    lon = config.gps_lon;
                }
                double dtmp = aprsParse.direction(lon, lat, aprs.lng, aprs.lat);
                double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                // if (config.h_up == true) {
                // 	//double course = gps.course.deg();
                // 	double course = SB_HEADING;
                // 	if (dtmp>=course) {
                // 		dtmp -= course;
                // 	}
                // 	else {
                // 		double diff = dtmp-course;
                // 		dtmp = diff+360.0F;
                // 	}
                // 	compass_label(25, 37, 15, course, WHITE);
                // 	display.setCursor(0, 17);
                // 	display.printf("H");
                // }
                // else {
                compass_label(25, 37, 15, 0.0F, WHITE);
                //}
                compass_arrow(25, 37, 12, dtmp, WHITE);
                display.drawFastHLine(1, 55, 45, WHITE);
                display.drawFastVLine(1, 55, 5, WHITE);
                display.drawFastVLine(46, 55, 5, WHITE);
                display.setCursor(4, 57);
                if (dist > 999)
                    display.printf("%.fKm", dist);
                else
                    display.printf("%.1fKm", dist);
                if (aprs.flags & F_CSRSPD) {
                    display.setCursor(51, x += 9);
                    // display.printf("SPD %d/", aprs.course);
                    // display.setCursor(50, x += 9);
                    display.printf("SPD %.1fkPh\n", aprs.speed);
                    int dirIdx = ((aprs.course + 22) / 45) % 8;
                    if (dirIdx > 8)
                        dirIdx = 8;
                    display.setCursor(51, x += 9);
                    display.printf("CSD %d(%s)", aprs.course, directions[dirIdx]);
                }
                if (aprs.flags & F_ALT) {
                    display.setCursor(51, x += 9);
                    display.printf("ALT %.1fM\n", aprs.altitude);
                }
                if (aprs.flags & F_PHG) {
                    int power, height, gain;
                    unsigned char tmp;
                    power = (int)aprs.phg[0] - 0x30;
                    power *= power;
                    height = (int)aprs.phg[1] - 0x30;
                    height = 10 << (height + 1);
                    height = height / 3.2808;
                    gain = (int)aprs.phg[2] - 0x30;
                    display.setCursor(51, x += 9);
                    display.printf("PHG %dM.\n", height);
                    display.setCursor(51, x += 9);
                    display.printf("PWR %dWatt\n", power);
                    display.setCursor(51, x += 9);
                    display.printf("ANT %ddBi\n", gain);
                }
                if (aprs.flags & F_RNG) {
                    display.setCursor(51, x += 9);
                    display.printf("RNG %dKm\n", aprs.radio_range);
                }
                /*if (aprs.comment_len > 0) {
                    display.setCursor(0, 56);
                    display.print(aprs.comment);
                }*/
            }
            display.display();
        }
    }
#else
    uint16_t bgcolor, txtcolor;
    bool Monitor = false;
    char text[200];
    unsigned char x = 0;
    char itemname[10];
    int start_val = line.indexOf(">", 0); // หาตำแหน่งแรกของ >
    if (start_val > 3) {
        // Serial.println(line);
        String src_call = line.substring(0, start_val);
        memset(&aprs, 0, sizeof(pbuf_t));
        aprs.buf_len = 300;
        aprs.packet_len = line.length();
        line.toCharArray(&aprs.data[0], aprs.packet_len);
        int start_info = line.indexOf(":", 0);
        int end_ssid = line.indexOf(",", 0);
        int start_dst = line.indexOf(">", 2);
        int start_dstssid = line.indexOf("-", start_dst);
        if ((start_dstssid > start_dst) && (start_dstssid < start_dst + 10)) {
            aprs.dstcall_end_or_ssid = &aprs.data[start_dstssid];
        } else {
            aprs.dstcall_end_or_ssid = &aprs.data[end_ssid];
        }
        aprs.info_start = &aprs.data[start_info + 1];
        aprs.dstname = &aprs.data[start_dst + 1];
        aprs.dstname_len = end_ssid - start_dst;
        aprs.dstcall_end = &aprs.data[end_ssid];
        aprs.srccall_end = &aprs.data[start_dst];

        // Serial.println(aprs.info_start);
        // aprsParse.parse_aprs(&aprs);
        if (aprsParse.parse_aprs(&aprs)) {
            if (filter == true) {
                if ((aprs.packettype & T_STATUS)) {
                    Monitor = true;
                } else if ((aprs.packettype & T_MESSAGE)) {
                    Monitor = true;
                } else if ((aprs.packettype & T_TELEMETRY)) {
                    Monitor = true;
                } else if ((aprs.packettype & T_WX)) {
                    Monitor = true;
                }

                if ((aprs.packettype & T_POSITION)) {
                    double lat, lon;
                    if (gps.location.isValid()) {
                        lat = gps.location.lat();
                        lon = gps.location.lng();
                    } else {
                        lat = config.gps_lat;
                        lon = config.gps_lon;
                    }
                    double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);

                    Monitor = true;
                }

                if ((aprs.packettype & T_POSITION)) {
                    if (aprs.flags & F_CSRSPD) {
                        double lat, lon;
                        if (gps.location.isValid()) {
                            lat = gps.location.lat();
                            lon = gps.location.lng();
                        } else {
                            lat = config.gps_lat;
                            lon = config.gps_lon;
                        }
                        double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                        Monitor = true;
                    }
                }

                if ((aprs.packettype & T_POSITION)) {
                    if (aprs.flags & F_CSRSPD) {
                        if (aprs.speed > 0) {
                            double lat, lon;
                            if (gps.location.isValid()) {
                                lat = gps.location.lat();
                                lon = gps.location.lng();
                            } else {
                                lat = config.gps_lat;
                                lon = config.gps_lon;
                            }
                            double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                            Monitor = true;
                        }
                    }
                }
            } else {
                Monitor = true;
            }
        } else {
            return;
        }

        if (Monitor) {
            if (dispPush) {
                disp_delay = 600 * 1000;
            } else {
                disp_delay = 3 * 1000;
            }
            timeHalfSec = millis() + disp_delay;
            const uint8_t *ptrSymbol;
            uint8_t symIdx = aprs.symbol[1] - 0x21;
            if (symIdx > 95)
                symIdx = 0;
            if (aprs.symbol[0] == '/') {
                ptrSymbol = &Icon_TableA[symIdx][0];
            } else if (aprs.symbol[0] == '\\') {
                ptrSymbol = &Icon_TableB[symIdx][0];
            } else {
                if (aprs.symbol[0] < 'A' || aprs.symbol[0] > 'Z') {
                    aprs.symbol[0] = 'N';
                    aprs.symbol[1] = '&';
                    symIdx = 5; // &
                }
                ptrSymbol = &Icon_TableB[symIdx][0];
            }
            if (!(aprs.symbol[0] == '/' || aprs.symbol[0] == '\\')) {
                Serial.printf("Symbol: %c\r\n", aprs.symbol[0]);
            }

            if (aprs.srcname_len > 0) {
                memset(&itemname, 0, sizeof(itemname));
                memcpy(&itemname, aprs.srcname, aprs.srcname_len);
                Serial.println(itemname);
            } else {
                Serial.println(src_call);
            }

            // display.print(selTab);

            if (mode == 1) {
                Serial.println("TNC2 RAW");
                Serial.println(line);
                return;
            }

            if (aprs.packettype & T_TELEMETRY) {
                bool show = false;
                int idx = tlmList_Find((char *)src_call.c_str());
                if (idx < 0) {
                    idx = tlmListOld();
                    if (idx > -1)
                        memset(&Telemetry[idx], 0, sizeof(Telemetry_struct));
                }
                if (idx > -1) {
                    Telemetry[idx].time = now();
                    strcpy(Telemetry[idx].callsign, (char *)src_call.c_str());

                    // for (int i = 0; i < 3; i++) Telemetry[idx].UNIT[i][5] = 0;
                    if (aprs.flags & F_UNIT) {
                        memcpy(Telemetry[idx].UNIT, aprs.tlm_unit.val, sizeof(Telemetry[idx].UNIT));
                    } else if (aprs.flags & F_PARM) {
                        memcpy(Telemetry[idx].PARM, aprs.tlm_parm.val, sizeof(Telemetry[idx].PARM));
                    } else if (aprs.flags & F_EQNS) {
                        Telemetry[idx].EQNS_FLAG = true;
                        for (int i = 0; i < 15; i++)
                            Telemetry[idx].EQNS[i] = aprs.tlm_eqns.val[i];
                    } else if (aprs.flags & F_BITS) {
                        Telemetry[idx].BITS_FLAG = aprs.telemetry.bitsFlag;
                    } else if (aprs.flags & F_TLM) {
                        for (int i = 0; i < 5; i++)
                            Telemetry[idx].RAW[i] = aprs.telemetry.val[i];
                        Telemetry[idx].BITS = aprs.telemetry.bits;
                        show = true;
                    }

                    for (int i = 0; i < 4; i++) { // Cut length
                        if (strstr(Telemetry[idx].PARM[i], "RxTraffic") != 0)
                            sprintf(Telemetry[idx].PARM[i], "RX");
                        if (strstr(Telemetry[idx].PARM[i], "TxTraffic") != 0)
                            sprintf(Telemetry[idx].PARM[i], "TX");
                        if (strstr(Telemetry[idx].PARM[i], "RxDrop") != 0)
                            sprintf(Telemetry[idx].PARM[i], "DROP");
                        Telemetry[idx].PARM[i][6] = 0;
                        Telemetry[idx].UNIT[i][3] = 0;
                        for (int a = 0; a < 3; a++) {
                            if (Telemetry[idx].UNIT[i][a] == '/')
                                Telemetry[idx].UNIT[i][a] = 0;
                        }
                    }

                    for (int i = 0; i < 5; i++) {
                        if (Telemetry[idx].PARM[i][0] == 0) {
                            sprintf(Telemetry[idx].PARM[i], "CH%d", i + 1);
                        }
                    }
                }
                if (show || filter == false) {
                    Serial.println("TELEMETRY");
                    Serial.print(Telemetry[idx].PARM[0]);
                    Serial.print(":");

                    if (Telemetry[idx].EQNS_FLAG == true) {
                        for (int i = 0; i < 5; i++) {
                            // a*v^2+b*v+c
                            Telemetry[idx].VAL[i] = (Telemetry[idx].EQNS[(i * 3) + 0] * pow(Telemetry[idx].RAW[i], 2));
                            Telemetry[idx].VAL[i] += Telemetry[idx].EQNS[(i * 3) + 1] * Telemetry[idx].RAW[i];
                            Telemetry[idx].VAL[i] += Telemetry[idx].EQNS[(i * 3) + 2];
                        }
                    } else {
                        for (int i = 0; i < 5; i++) {
                            // a*v^2+b*v+c
                            Telemetry[idx].VAL[i] = Telemetry[idx].RAW[i];
                        }
                    }

                    if (fmod(Telemetry[idx].VAL[0], 1) == 0)
                        Serial.print(Telemetry[idx].VAL[0], 0);
                    else
                        Serial.print(Telemetry[idx].VAL[0], 1);
                    Serial.println(Telemetry[idx].UNIT[0]);

                    Serial.print(Telemetry[idx].PARM[1]);
                    Serial.print(":");
                    if (fmod(Telemetry[idx].VAL[1], 1) == 0)
                        Serial.print(Telemetry[idx].VAL[1], 0);
                    else
                        Serial.print(Telemetry[idx].VAL[1], 1);
                    Serial.println(Telemetry[idx].UNIT[1]);

                    Serial.print(Telemetry[idx].PARM[2]);
                    Serial.print(":");
                    if (fmod(Telemetry[idx].VAL[2], 1) == 0)
                        Serial.print(Telemetry[idx].VAL[2], 0);
                    else
                        Serial.print(Telemetry[idx].VAL[2], 1);
                    Serial.println(Telemetry[idx].UNIT[2]);

                    Serial.print(Telemetry[idx].PARM[3]);
                    Serial.print(":");
                    if (fmod(Telemetry[idx].VAL[3], 1) == 0)
                        Serial.print(Telemetry[idx].VAL[3], 0);
                    else
                        Serial.print(Telemetry[idx].VAL[3], 1);
                    Serial.println(Telemetry[idx].UNIT[3]);

                    Serial.print(Telemetry[idx].PARM[4]);
                    Serial.print(":");
                    Serial.print(Telemetry[idx].VAL[4], 1);
                    Serial.println(Telemetry[idx].UNIT[4]);

                    Serial.print("BIT ");
                    uint8_t bit = Telemetry[idx].BITS;
                    for (int i = 0; i < 8; i++) {
                        if (bit & 0x80) {
                            Serial.print("1 ");
                        } else {
                            Serial.print("0 ");
                        }
                        bit <<= 1;
                    }
                    Serial.println();
                }
                return;
            } else if (aprs.packettype & T_STATUS) {
                Serial.println("STATUS");
                Serial.println(aprs.comment);
                return;
            } else if (aprs.packettype & T_QUERY) {
                Serial.println("?QUERY?");
                Serial.println(aprs.comment);
                return;
            } else if (aprs.packettype & T_MESSAGE) {
                if (aprs.msg.is_ack == 1) {
                } else if (aprs.msg.is_rej == 1) {
                } else {
                    Serial.println("MESSAGE");
                    Serial.print("{");
                    strncpy(&text[0], aprs.msg.msgid, aprs.msg.msgid_len);
                    int msgid = atoi(text);
                    Serial.print(msgid, DEC);
                    Serial.println("}");
                    Serial.print("To: ");
                    strncpy(&text[0], aprs.dstname, aprs.dstname_len);
                    Serial.println(text);
                    String mycall;
                    if (config.aprs_ssid == 0)
                        mycall = config.aprs_mycall;
                    else
                        mycall = config.aprs_mycall + String("-") + String(config.aprs_ssid, DEC);
                    if (strcmp(mycall.c_str(), text) == 0) {
                        Serial.print("ACK:");
                        Serial.println(msgid);
                        sendIsAckMsg(src_call, msgid);
                    }
                    strncpy(&text[0], aprs.msg.body, aprs.msg.body_len);
                    Serial.print("Msg: ");
                    Serial.println(text);
                }
                return;
            }
            x = 8;

            if (aprs.srcname_len > 0) {
                x += 9;
                Serial.println("By " + src_call);
                // x += 9;
            }
            if (aprs.packettype & T_WX) {
                // Serial.println("WX Display");
                if (aprs.wx_report.flags & W_TEMP) {
                    Serial.printf("%.1fC\r\n", aprs.wx_report.temp);
                }
                if (aprs.wx_report.flags & W_HUM) {
                    Serial.printf("%d%%\r\n", aprs.wx_report.humidity);
                }
                if (aprs.wx_report.flags & W_BAR) {
                    Serial.printf("%.1fhPa\r\n", aprs.wx_report.pressure);
                }
                if (aprs.wx_report.flags & W_R24H) {
                    Serial.printf("%.1fmm.\r\n", aprs.wx_report.rain_24h);
                }
                if (aprs.wx_report.flags & W_PAR) {
                    Serial.printf("%dW/m\r\n", aprs.wx_report.luminosity);
                    if (aprs.wx_report.flags & W_UV) {
                        Serial.printf(" UV%d\r\n", aprs.wx_report.uv);
                    }
                }
                if (aprs.wx_report.flags & W_WS) {
                    int dirIdx = ((aprs.wx_report.wind_dir + 22) / 45) % 8;
                    if (dirIdx > 8)
                        dirIdx = 8;
                    Serial.printf("%.1fkPh(%s)\r\n", aprs.wx_report.wind_speed, directions[dirIdx]);
                }
                // Serial.printf("%.1fkPh(%d)", aprs.wx_report.wind_speed, aprs.wx_report.wind_dir);
                if (aprs.flags & F_HASPOS) {
                    // Serial.println("POS Display");
                    double lat, lon;
                    if (gps.location.isValid()) {
                        lat = gps.location.lat();
                        lon = gps.location.lng();
                    } else {
                        lat = config.gps_lat;
                        lon = config.gps_lon;
                    }
                    double dtmp = aprsParse.direction(lon, lat, aprs.lng, aprs.lat);
                    double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                    if (dist > 999)
                        Serial.printf("%.fKm\r\n", dist);
                    else
                        Serial.printf("%.1fKm\r\n", dist);
                } else {
                    Serial.printf("NO POSITION\r\n");
                }
            } else if (aprs.flags & F_HASPOS) {
                String str;
                Serial.print("LAT:");
                str = String(aprs.lat, 5);
                Serial.println(str);

                Serial.print("LON:");
                str = String(aprs.lng, 5);
                Serial.println(str);

                double lat, lon;
                if (gps.location.isValid()) {
                    lat = gps.location.lat();
                    lon = gps.location.lng();
                } else {
                    lat = config.gps_lat;
                    lon = config.gps_lon;
                }
                double dtmp = aprsParse.direction(lon, lat, aprs.lng, aprs.lat);
                double dist = aprsParse.distance(lon, lat, aprs.lng, aprs.lat);
                Serial.print("DIST:");
                if (dist > 999)
                    Serial.printf("%.fKm\r\n", dist);
                else
                    Serial.printf("%.1fKm\r\n", dist);
                if (aprs.flags & F_CSRSPD) {
                    Serial.printf("SPD %.1fkPh\r\n", aprs.speed);
                    int dirIdx = ((aprs.course + 22) / 45) % 8;
                    if (dirIdx > 8)
                        dirIdx = 8;
                    Serial.printf("CSD %d(%s)\r\n", aprs.course, directions[dirIdx]);
                }
                if (aprs.flags & F_ALT) {
                    Serial.printf("ALT %.1fM\r\n", aprs.altitude);
                }
                if (aprs.flags & F_PHG) {
                    int power, height, gain;
                    unsigned char tmp;
                    power = (int)aprs.phg[0] - 0x30;
                    power *= power;
                    height = (int)aprs.phg[1] - 0x30;
                    height = 10 << (height + 1);
                    height = height / 3.2808;
                    gain = (int)aprs.phg[2] - 0x30;
                    Serial.printf("PHG %dM.\n", height);
                    Serial.printf("PWR %dWatt\n", power);
                    Serial.printf("ANT %ddBi\n", gain);
                }
                if (aprs.flags & F_RNG) {
                    Serial.printf("RNG %dKm\n", aprs.radio_range);
                }
                /*if (aprs.comment_len > 0) {
                    Serial.println(aprs.comment);
                }*/
            }
        }
    }
#endif
}

void statisticsDisp() {
    // uint8 wifi = 0, i;
    int x;
    String str;
#ifdef NEW_OLED
    // display.fillRect(0, 16, 128, 10, WHITE);
    // display.drawLine(0, 16, 0, 63, WHITE);
    // display.drawLine(127, 16, 127, 63, WHITE);
    // display.drawLine(0, 63, 127, 63, WHITE);
    // display.fillRect(1, 25, 126, 38, BLACK);
    // display.setTextColor(BLACK);
    // display.setCursor(30, 17);
    // display.print("STATISTICS");
    // display.setCursor(108, 17);
    // display.print("1/5");
    // display.setTextColor(WHITE);

    display.fillRect(0, 0, 128, 15, WHITE);
    display.drawRect(0, 16, 128, 48, WHITE);
    display.fillRect(1, 17, 126, 46, BLACK);

    display.setCursor(5, 7);
    display.setTextSize(1);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(BLACK);
    display.print("STATISTIC");
    display.setFont();
    display.setCursor(108, 7);
    display.print("1/4");
    display.setTextColor(WHITE);

    display.setCursor(3, 18);
    display.print("ALL DATA");
    str = String(status.allCount, DEC);
    x = str.length() * 6;
    display.setCursor(126 - x, 18);
    display.print(str);

    display.setCursor(3, 26);
    display.print("DIGI RPT");
    str = String(status.digiCount, DEC);
    x = str.length() * 6;
    display.setCursor(126 - x, 26);
    display.print(str);

    display.setCursor(3, 35);
    display.print("RF->INET");
    str = String(status.rf2inet, DEC);
    x = str.length() * 6;
    display.setCursor(126 - x, 35);
    display.print(str);

    display.setCursor(3, 44);
    display.print("INET->RF");
    str = String(status.inet2rf, DEC);
    x = str.length() * 6;
    display.setCursor(126 - x, 44);
    display.print(str);

    display.setCursor(3, 53);
    display.print("ERROR/DROP");
    str = String(status.errorCount + status.dropCount, DEC);
    x = str.length() * 6;
    display.setCursor(126 - x, 53);
    display.print(str);

    display.display();
#else
    Serial.println("STATISTIC");
    Serial.println("ALL DATA");
    str = String(status.allCount, DEC);
    Serial.println(str);

    Serial.println("DIGI RPT");
    str = String(status.digiCount, DEC);
    Serial.println(str);

    Serial.println("RF->INET");
    str = String(status.rf2inet, DEC);
    Serial.println(str);

    Serial.println("INET->RF");
    str = String(status.inet2rf, DEC);
    Serial.println(str);

    Serial.println("ERROR/DROP");
    str = String(status.errorCount + status.dropCount, DEC);
    Serial.println(str);
#endif
}

void pkgLastDisp() {
    uint8_t k = 0;
    int i;
    // char list[4];
    int x, y;
    String str;
    // String times;
    // pkgListType *ptr[100];

#ifdef NEW_OLED
    display.fillRect(0, 0, 128, 15, WHITE);
    display.drawRect(0, 16, 128, 48, WHITE);
    display.fillRect(1, 17, 126, 46, BLACK);

    display.setCursor(15, 7);
    display.setTextSize(1);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(BLACK);
    display.print("STATION");
    display.setFont();
    display.setCursor(108, 7);
    display.print("2/4");
    display.setTextColor(WHITE);

    // display.fillRect(0, 16, 128, 10, WHITE);
    // display.drawLine(0, 16, 0, 63, WHITE);
    // display.drawLine(127, 16, 127, 63, WHITE);
    // display.drawLine(0, 63, 127, 63, WHITE);
    // display.fillRect(1, 25, 126, 38, BLACK);
    // display.setTextColor(BLACK);
    // display.setCursor(27, 17);
    // display.print("LAST STATIONS");
    // display.setCursor(108, 17);
    // display.print("2/5");
    // display.setTextColor(WHITE);

    sort(pkgList, PKGLISTSIZE);
    k = 0;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if (pkgList[i].time > 0) {
            y = 18 + (k * 9);
            // display.drawBitmap(3, y, &SYMBOL[0][0], 11, 6, WHITE);
            display.fillRoundRect(2, y, 7, 8, 2, WHITE);
            display.setCursor(3, y);
            pkgList[i].calsign[10] = 0;
            display.setTextColor(BLACK);
            switch (pkgList[i].type) {
            case PKG_OBJECT:
                display.print("O");
                break;
            case PKG_ITEM:
                display.print("I");
                break;
            case PKG_MESSAGE:
                display.print("M");
                break;
            case PKG_WX:
                display.print("W");
                break;
            case PKG_TELEMETRY:
                display.print("T");
                break;
            case PKG_QUERY:
                display.print("Q");
                break;
            case PKG_STATUS:
                display.print("S");
                break;
            default:
                display.print("*");
                break;
            }
            display.setTextColor(WHITE);
            display.setCursor(10, y);
            display.print(pkgList[i].calsign);
            display.setCursor(126 - 48, y);
            // display.printf("%02d:%02d:%02d", hour(pkgList[i].time), minute(pkgList[i].time), second(pkgList[i].time));

            // time_t tm = pkgList[i].time;
            struct tm tmstruct;
            localtime_r(&pkgList[i].time, &tmstruct);
            String str = String(tmstruct.tm_hour, DEC) + ":" + String(tmstruct.tm_min, DEC) + ":" + String(tmstruct.tm_sec, DEC);
            display.print(str);
            // str = String(hour(pkgList[i].time),DEC) + ":" + String(minute(pkgList[i].time), DEC) + ":" + String(second(pkgList[i].time), DEC);
            ////str = String(pkgList[pkgLast_array[i]].time, DEC);
            // x = str.length() * 6;
            // display.setCursor(126 - x, y);
            // display.print(str);
            k++;
            if (k >= 5)
                break;
        }
    }
    display.display();
#else
    Serial.println("STATION");
    sort(pkgList, PKGLISTSIZE);
    k = 0;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if (pkgList[i].time > 0) {
            y = 18 + (k * 9);
            pkgList[i].calsign[10] = 0;
            switch (pkgList[i].type) {
            case PKG_OBJECT:
                Serial.println("O");
                break;
            case PKG_ITEM:
                Serial.println("I");
                break;
            case PKG_MESSAGE:
                Serial.println("M");
                break;
            case PKG_WX:
                Serial.println("W");
                break;
            case PKG_TELEMETRY:
                Serial.println("T");
                break;
            case PKG_QUERY:
                Serial.println("Q");
                break;
            case PKG_STATUS:
                Serial.println("S");
                break;
            default:
                Serial.println("*");
                break;
            }
            Serial.println(pkgList[i].calsign);
            // Serial.printf("%02d:%02d:%02d", hour(pkgList[i].time), minute(pkgList[i].time), second(pkgList[i].time));

            // time_t tm = pkgList[i].time;
            struct tm tmstruct;
            localtime_r(&pkgList[i].time, &tmstruct);
            String str = String(tmstruct.tm_hour, DEC) + ":" + String(tmstruct.tm_min, DEC) + ":" + String(tmstruct.tm_sec, DEC);
            Serial.println(str);
            // str = String(hour(pkgList[i].time),DEC) + ":" + String(minute(pkgList[i].time), DEC) + ":" + String(second(pkgList[i].time), DEC);
            ////str = String(pkgList[pkgLast_array[i]].time, DEC);
            // x = str.length() * 6;
            // Serial.print(str);
            k++;
            if (k >= 5)
                break;
        }
    }
#endif
}

void pkgCountDisp() {
    // uint8 wifi = 0, k = 0, l;
    uint k = 0;
    int i;
    // char list[4];
    int x, y;
    String str;
    // String times;
    // pkgListType *ptr[100];

#ifdef NEW_OLED
    display.fillRect(0, 0, 128, 15, WHITE);
    display.drawRect(0, 16, 128, 48, WHITE);
    display.fillRect(1, 17, 126, 46, BLACK);

    display.setCursor(20, 7);
    display.setTextSize(1);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(BLACK);
    display.print("TOP PKG");
    display.setFont();
    display.setCursor(108, 7);
    display.print("3/4");
    display.setTextColor(WHITE);

    // display.setCursor(3, 18);

    // display.fillRect(0, 16, 128, 10, WHITE);
    // display.drawLine(0, 16, 0, 63, WHITE);
    // display.drawLine(127, 16, 127, 63, WHITE);
    // display.drawLine(0, 63, 127, 63, WHITE);
    // display.fillRect(1, 25, 126, 38, BLACK);
    // display.setTextColor(BLACK);
    // display.setCursor(30, 17);
    // display.print("TOP PACKAGE");
    // display.setCursor(108, 17);
    // display.print("3/5");
    // display.setTextColor(WHITE);

    sortPkgDesc(pkgList, PKGLISTSIZE);
    k = 0;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if (pkgList[i].time > 0) {
            y = 18 + (k * 9);
            // display.drawBitmapV(2, y-1, &SYMBOL[pkgList[i].symbol][0], 11, 8, WHITE);
            pkgList[i].calsign[10] = 0;
            display.fillRoundRect(2, y, 7, 8, 2, WHITE);
            display.setCursor(3, y);
            pkgList[i].calsign[10] = 0;
            display.setTextColor(BLACK);
            switch (pkgList[i].type) {
            case PKG_OBJECT:
                display.print("O");
                break;
            case PKG_ITEM:
                display.print("I");
                break;
            case PKG_MESSAGE:
                display.print("M");
                break;
            case PKG_WX:
                display.print("W");
                break;
            case PKG_TELEMETRY:
                display.print("T");
                break;
            case PKG_QUERY:
                display.print("Q");
                break;
            case PKG_STATUS:
                display.print("S");
                break;
            default:
                display.print("*");
                break;
            }
            display.setTextColor(WHITE);
            display.setCursor(10, y);
            display.print(pkgList[i].calsign);
            str = String(pkgList[i].pkg, DEC);
            x = str.length() * 6;
            display.setCursor(126 - x, y);
            display.print(str);
            k++;
            if (k >= 5)
                break;
        }
    }
    display.display();
#else
    Serial.println("TOP PKG");

    sortPkgDesc(pkgList, PKGLISTSIZE);
    k = 0;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if (pkgList[i].time > 0) {
            y = 18 + (k * 9);
            pkgList[i].calsign[10] = 0;
            pkgList[i].calsign[10] = 0;
            switch (pkgList[i].type) {
            case PKG_OBJECT:
                Serial.println("O");
                break;
            case PKG_ITEM:
                Serial.println("I");
                break;
            case PKG_MESSAGE:
                Serial.println("M");
                break;
            case PKG_WX:
                Serial.println("W");
                break;
            case PKG_TELEMETRY:
                Serial.println("T");
                break;
            case PKG_QUERY:
                Serial.println("Q");
                break;
            case PKG_STATUS:
                Serial.println("S");
                break;
            default:
                Serial.println("*");
                break;
            }
            Serial.println(pkgList[i].calsign);
            str = String(pkgList[i].pkg, DEC);
            x = str.length() * 6;
            Serial.println(str);
            k++;
            if (k >= 5)
                break;
        }
    }
#endif
}

void systemDisp() {
    // uint8 wifi = 0, k = 0, l;
    // char i;
    // char list[4];
    int x;
    String str;
    time_t upTime = now() - systemUptime;
    // String times;
    // pkgListType *ptr[100];

#ifdef NEW_OLED
    // display.fillRect(0, 16, 128, 10, WHITE);
    // display.drawLine(0, 16, 0, 63, WHITE);
    display.fillRect(0, 0, 128, 15, WHITE);
    // display.drawLine(0, 16, 0, 63, WHITE);

    // display.drawLine(127, 16, 127, 63, WHITE);
    // display.drawLine(0, 63, 127, 63, WHITE);
    // display.fillRect(1, 25, 126, 38, BLACK);
    display.drawRect(0, 16, 128, 48, WHITE);
    display.fillRect(1, 17, 126, 46, BLACK);
    // display.fillRoundRect(1, 17, 126, 46, 2, WHITE);

    display.setCursor(20, 7);
    display.setTextSize(1);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(BLACK);
    display.print("SYSTEM");
    display.setFont();
    display.setCursor(108, 7);
    display.print("4/4");
    display.setTextColor(WHITE);

    display.setCursor(3, 18);
    // display.print("HMEM:");
    display.print("MAC");
    // str = String(ESP.getFreeHeap(), DEC)+"Byte";
    str = String(WiFi.macAddress());
    x = str.length() * 6;
    display.setCursor(126 - x, 18);
    display.print(str);

    display.setCursor(3, 26);
    display.print("IP:");
    str = String(WiFi.localIP().toString());
    x = str.length() * 6;
    display.setCursor(126 - x, 26);
    display.print(str);

    display.setCursor(3, 35);
    display.print("UPTIME:");
    str = String(day(upTime) - 1, DEC) + "D " + String(hour(upTime), DEC) + ":" + String(minute(upTime), DEC) + ":" + String(second(upTime), DEC);
    x = str.length() * 6;
    display.setCursor(126 - x, 35);
    display.print(str);

    display.setCursor(3, 44);
    display.print("WiFi RSSI:");
    str = String(WiFi.RSSI()) + "dBm";
    // str = String(str_status[WiFi.status()]);
    x = str.length() * 6;
    display.setCursor(126 - x, 44);
    display.print(str);

    display.setCursor(3, 53);
    display.print("Firmware:");
    str = "V" + String(VERSION);
    x = str.length() * 6;
    display.setCursor(126 - x, 53);
    display.print(str);

    display.display();
#else
    Serial.println("SYSTEM");

    Serial.print("MAC: ");
    // str = String(ESP.getFreeHeap(), DEC)+"Byte";
    str = String(WiFi.macAddress());
    Serial.println(str);

    Serial.print("IP: ");
    str = String(WiFi.localIP().toString());
    Serial.println(str);

    Serial.print("UPTIME: ");
    str = String(day(upTime) - 1, DEC) + "D " + String(hour(upTime), DEC) + ":" + String(minute(upTime), DEC) + ":" + String(second(upTime), DEC);
    Serial.println(str);

    Serial.print("WiFi RSSI: ");
    str = String(WiFi.RSSI()) + "dBm";
    // str = String(str_status[WiFi.status()]);
    Serial.println(str);

    Serial.print("Firmware: ");
    str = "V" + String(VERSION);
    Serial.println(str);
#endif
}
