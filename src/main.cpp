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

statusType status;
RTC_DATA_ATTR igateTLMType igateTLM;
RTC_DATA_ATTR txQueueType txQueue[PKGTXSIZE];

extern RTC_DATA_ATTR uint8_t digiCount;

Configuration config;

TaskHandle_t taskNetworkHandle;
TaskHandle_t taskAPRSHandle;

#if defined(USE_TNC)
HardwareSerial SerialTNC(SERIAL_TNC_UART);
#endif

#if defined(USE_GPS)
TinyGPSPlus gps;
HardwareSerial SerialGPS(SERIAL_GPS_UART);
#endif

BluetoothSerial SerialBT;

#if defined(USE_SCREEN_SSD1306)
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST_PIN);
#elif defined(USE_SCREEN_SH1106)
Adafruit_SH1106 display(OLED_SDA_PIN, OLED_SCL_PIN);
#endif

#ifdef USE_ROTARY
Rotary rotary = Rotary(PIN_ROT_CLK, PIN_ROT_DT, PIN_ROT_BTN);
#endif

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

    Serial.println();
    Serial.println("Start APRS-ESP V" + String(VERSION));
    Serial.println("Press and hold BOOT button for 3 sec to Factory Default config");

#ifdef USE_SCREEN
    OledStartup();
#endif

    LoadConfig();

#ifdef USE_RF
    RF_Init(true);
#endif

    // if SmartBeaconing - delay processing GPS data
    if (config.aprs_beacon == 0) {
        Serial.println("SmartBeaconing, delayed GPS processing");
        gpsUpdTMO = 30000;  // 30 sec
    }

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
                            32768,         /* Stack size in words */
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

void loop()
{
    vTaskDelay(5 / portTICK_PERIOD_MS);  // 5 ms // remove?
    if (millis() > timeCheck) {
        timeCheck = millis() + 10000;
        if (ESP.getFreeHeap() < 70000) esp_restart();
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

    updateScreenAndGps(update_screen);
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

    sendTimer = millis() - (config.aprs_beacon * 1000) + 30000;
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

        if (digitalRead(BOOT_PIN) == LOW) {
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
            sendByTime = (now > (sendTimer + (config.aprs_beacon * 1000)));
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

                // IGate Process
                if (config.rf2inet && aprsClient.connected()) {
                    int ret = igateProcess(incomingPacket);
                    if (ret == 0) {
                        status.dropCount++;
                        igateTLM.DROP++;
                    } else {
                        status.rf2inet++;
                        igateTLM.RF2INET++;
                        igateTLM.TX++;
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
                        pkgListUpdate(call, 1);
                    }
                }

                // Digi Repeater Process
                if (config.tnc_digi) {
                    int dlyFlag = digiProcess(incomingPacket);
                    if (dlyFlag > 0) {
                        int digiDelay;
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
        Serial.println(F("WiFi OFF. BT only mode"));
        SerialBT.begin("ESP32TNC");
    }

    webService();
    pingTimeout = millis() + 10000;

    for (;;) {
        // wdtNetworkTimer = millis();
        vTaskDelay(5 / portTICK_PERIOD_MS);
        serviceHandle();

        if (config.wifi_mode == WIFI_AP_STA_FIX ||
            config.wifi_mode == WIFI_STA_FIX) {
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
                            // pingTimeout = millis() + 300000; // Reset ping
                            // timout
                            String line = aprsClient.readStringUntil('\n');  //read the value at Server answer sleep line by line
#ifdef DEBUG_IS
                            printTime();
                            Serial.print("APRS-IS ");
                            Serial.println(line);
#endif
                            status.isCount++;
                            int start_val =
                                line.indexOf(">", 0);  // find the first position of >
                            if (start_val > 3) {
                                // raw = (char *)malloc(line.length() + 1);
                                String src_call = line.substring(0, start_val);
                                String msg_call = "::" + src_call;

                                status.allCount++;
                                igateTLM.RX++;
                                if (config.tnc && config.inet2rf) {
                                    if (line.indexOf(msg_call) <= 0)  // src callsign = msg callsign
                                    // Not a telemetry topic
                                    {
                                        if (line.indexOf(":T#") < 0)  // not telemetry
                                        {
                                            if (line.indexOf("::") > 0)  // text only
                                            {  // message only
                                                // raw[0] = '}';
                                                // line.toCharArray(&raw[1],
                                                // line.length()); tncTxEnable =
                                                // false; SerialTNC.flush();
                                                // SerialTNC.println(raw);
                                                pkgTxUpdate(line.c_str(), 0);
                                                // APRS_sendTNC2Pkt(line); //
                                                // Send out RF by TNC build in
                                                //  tncTxEnable = true;
                                                status.inet2rf++;
                                                igateTLM.INET2RF++;
                                                printTime();
#ifdef DEBUG
                                                Serial.print("INET->RF ");
                                                Serial.println(line);
#endif
                                            }
                                        }
                                    } else {
                                        igateTLM.DROP++;
                                        Serial.print(
                                            "INET Message TELEMETRY from ");
                                        Serial.println(src_call);
                                    }
                                }

                                // memset(&raw[0], 0, sizeof(raw));
                                // line.toCharArray(&raw[0], start_val + 1);
                                // raw[start_val + 1] = 0;
                                // pkgListUpdate(&raw[0], 0);
                                // free(raw);
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
