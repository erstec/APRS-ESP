/*
    Name:       APRS-ESP is APRS Internet Gateway / Tracker / Digipeater
    Created:    2022-10-10
    Author:     Ernest / LY3PH
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
#include <WiFi.h>
// #include <WiFiMulti.h>
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

#include "rfModem.h"
#include "aprsMsg.h"

#ifdef USE_GPS
#include "TinyGPS++.h"
#endif

#ifdef USE_ROTARY
#include <Rotary.h>
#endif

#ifdef USE_PMU
#include <XPowersLib.h>
XPowersPMU PMU;
#endif

#if defined(USE_NEOPIXEL)
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIXELS_PIN, NEO_GRB + NEO_KHZ800);
#endif

#include "rotaryProc.h"

#if defined(USE_SCREEN) || defined(USE_PMU)
#include "Wire.h"
#include "oled.h"
#endif

#include <WiFiClientSecure.h>

#include "AFSK.h"

#ifdef SDCARD
#include <SPI.h> //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)
#include "FS.h"
#include <LittleFS.h>
#define SDCARD_CS 13
#define SDCARD_CLK 14
#define SDCARD_MOSI 15
#define SDCARD_MISO 2
#endif

#if defined(TX_LED_PIN)

#if defined(INVERT_LEDS)
#if defined(USE_NEOPIXEL)
#define TX_LED_ON() digitalWrite(TX_LED_PIN, LOW); strip.setPixelColor(0, 255, 0, 0);   // Red
#define TX_LED_OFF() digitalWrite(TX_LED_PIN, HIGH); strip.setPixelColor(0, 0, 0, 0);   // Off
#else
#define TX_LED_ON() if (TX_LED_PIN > -1) digitalWrite(TX_LED_PIN, LOW);
#define TX_LED_OFF() if (TX_LED_PIN > -1) digitalWrite(TX_LED_PIN, HIGH);
#endif
#else
#if defined(USE_NEOPIXEL)
#define TX_LED_ON() strip.setPixelColor(0, 255, 0, 0);  // Red
#define TX_LED_OFF() strip.setPixelColor(0, 0, 0, 0);   // Off
#else
#define TX_LED_ON() digitalWrite(TX_LED_PIN, HIGH);
#define TX_LED_OFF() digitalWrite(TX_LED_PIN, LOW);
#endif
#endif

#else

#if defined(USE_NEOPIXEL)
#define TX_LED_ON() strip.setPixelColor(0, 255, 0, 0);  // Red
#define TX_LED_OFF() strip.setPixelColor(0, 0, 0, 0);   // Off
#endif

#endif

HardwareSerial SerialRF(SERIAL_RF_UART);

bool fwUpdateProcess = false;
volatile bool menuUpdateNeeded = false;
volatile bool oledTxBusy       = false;

bool callsignValid = false;

#if defined(DOARD_TTWR)
#else
RTC_DATA_ATTR time_t systemUptime = 0;
#endif
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

cppQueue PacketBuffer(sizeof(AX25Msg), 10, IMPLEMENTATION);

statusType status;
char     lastRxCall[16] = "---";
time_t   lastRxTime     = 0;
bool     lastRxIsIS     = false;
RTC_DATA_ATTR igateTLMType igateTLM;

txQueueType *txQueue;

extern RTC_DATA_ATTR uint8_t digiCount;

Configuration config;

TaskHandle_t taskNetworkHandle;
TaskHandle_t taskAPRSHandle;
TaskHandle_t taskOLEDDisplayHandle;
TaskHandle_t taskGPSHandle;
#if !defined(BOARD_ESP32DR)
TaskHandle_t taskTNCHandle;
#endif

TelemetryType *Telemetry;

#if defined(USE_GPS)
TinyGPSPlus gps;
HardwareSerial SerialGPS(SERIAL_GPS_UART);
#endif

// BluetoothSerial SerialBT;

#ifdef USE_ROTARY
Rotary rotary = Rotary(PIN_ROT_CLK, PIN_ROT_DT, PIN_ROT_BTN);
void IRAM_ATTR rotaryISR() { rotary.processISR(); }
#endif

struct pbuf_t aprs;
ParseAPRS aprsParse;

// Set your Static IP address for wifi AP
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);  // must match local_IP — DHCP uses gateway as DNS server for clients
IPAddress subnet(255, 255, 255, 0);

int pkgTNC_count = 0;

unsigned long NTP_Timeout;

SemaphoreHandle_t psramMutex = NULL;

teTimeSync timeSyncFlag = T_SYNC_NONE;

static unsigned long gpsUpdTMO = 0;

bool AFSKInitAct = false;

int tlmList_Find(char *call) {
    int i;
    for (i = 0; i < TLMLISTSIZE; i++) {
        if (strstr(Telemetry[i].callsign, call) != NULL) {
            return i;
        }
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
        if (Telemetry[i].time > time(NULL))
            Telemetry[i].time = 0;
    }
    return ret;
}

TelemetryType getTlmList(int idx) {
    TelemetryType ret;
    if (xSemaphoreTake(psramMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        log_e("psramMutex timeout in getTlmList");
        memset(&ret, 0, sizeof(ret));
        return ret;
    }
    memcpy(&ret, &Telemetry[idx], sizeof(TelemetryType));
    xSemaphoreGive(psramMutex);
    return ret;
}

bool pkgTxSend() {
    if (getReceive())
        return false;
    if (xSemaphoreTake(psramMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        log_e("psramMutex timeout in pkgTxSend");
        return false;
    }
    char info[500];
    for (int i = 0; i < PKGTXSIZE; i++) {
        if (txQueue[i].Active) {
            int decTime = millis() - txQueue[i].timeStamp;
            if (decTime > txQueue[i].Delay) {
                txQueue[i].Active = false;
                memset(info, 0, sizeof(info));
                strcpy(info, txQueue[i].Info);
                xSemaphoreGive(psramMutex);
                digitalWrite(POWER_PIN, config.rf_power); // RF Power set
                status.txCount++;
                String _empty = "";
                String _msg = "TX";
                OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 1);
                vTaskDelay(60 / portTICK_PERIOD_MS); // let Core 1 render popup before locking I2C
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
                adcActive(false);
#endif
                TX_LED_ON();
                oledTxBusy = true;  // block taskOLEDDisplay from touching I2C during TX
                APRS_setPreamble(APRS_PREAMBLE);
                String _txPkt = String(info);
                _txPkt.trim();
                APRS_sendTNC2Pkt(_txPkt); // Send packet to RF
                log_i("[TX] {\"dir\":\"RF\",\"pkt\":\"%s\"}", _txPkt.c_str());

                for (int i = 0; i < 100; i++) {
#if defined(INVERT_PTT)
                    if (digitalRead(PTT_PIN) == 1) {
#else
                    if (digitalRead(PTT_PIN) == 0) {
#endif
                        log_i("[TX] {\"event\":\"ptt_release\"}");
                        break;
                    }
                    delay(50); // TOT 5sec
                }

                // delay(2000);
                TX_LED_OFF();
                digitalWrite(POWER_PIN, 0); // RF Power Low
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
                adcActive(true);
#endif
                oledTxBusy = false;
                
                return true;
            }
        }
    }
  
    xSemaphoreGive(psramMutex);

    return false;
}


bool pkgTxPush(const char *info, size_t len, int dly) {
    char *ecs = strstr(info, ">");
    if (ecs == NULL)
        return false;

    if (xSemaphoreTake(psramMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        log_e("psramMutex timeout in pkgTxPush");
        return false;
    }
    // for (int i = 0; i < PKGTXSIZE; i++)
    // {
    //   if (txQueue[i].Active)
    //   {
    //     if ((strncmp(&txQueue[i].Info[0], info, info - ecs)==0)) //Check src callsign
    //     {
    //       // strcpy(&txQueue[i].Info[0], info);
    //       memset(txQueue[i].Info, 0, sizeof(txQueue[i].Info));
    //       memcpy(&txQueue[i].Info[0], info, len);
    //       txQueue[i].Delay = dly;
    //       txQueue[i].timeStamp = millis();
    //       xSemaphoreGive(psramMutex);
    //       return true;
    //     }
    //   }
    // }

    // Add
    for (int i = 0; i < PKGTXSIZE; i++) {
        if (txQueue[i].Active == false) {
            memset(txQueue[i].Info, 0, sizeof(txQueue[i].Info));
            memcpy(&txQueue[i].Info[0], info, len);
            txQueue[i].Delay = dly;
            txQueue[i].Active = true;
            txQueue[i].timeStamp = millis();
            break;
        }
    }

    xSemaphoreGive(psramMutex);

    return true;
}

void aprsTimeGet(uint8_t *buf) {
    String msg = String((char *)buf);
    // Try get time from APRS message
    if (msg.length() < 10) return;
    int etaPos = msg.indexOf('@');
    if (etaPos < 0) {
        etaPos = msg.indexOf('*');
        if (etaPos < 0) return;
    }
    int tzpos = etaPos + 7;
    if (tzpos > msg.length()) return;
    if (msg[tzpos] != 'z') return;
    String time = msg.substring(etaPos + 1, etaPos + 7); // ddhhmm
    int day = time.substring(0, 2).toInt();
    int hour = time.substring(2, 4).toInt();
    int min = time.substring(4, 6).toInt();

    log_i("APRS Time: %02d %02d:%02d", day, hour, min);

    if (config.synctime && timeSyncFlag == T_SYNC_NONE && WiFi.status() != WL_CONNECTED) {
        // set internal rtc time
        struct tm tmstruct;
        getLocalTime(&tmstruct, 0);
        tmstruct.tm_mday = day;
        tmstruct.tm_hour = hour;
        tmstruct.tm_min = min;
        tmstruct.tm_sec = 0;
        time_t t = mktime(&tmstruct);
        setTime(t);
        // adjust locat timezone
        // adjustTime(config.timeZone * SECS_PER_HOUR);

        timeval tv;
        tv.tv_sec = hour * 3600 + min * 60;
        tv.tv_usec = 0;
        
        timezone tz;
        tz.tz_minuteswest = config.timeZone * 60;
        tz.tz_dsttime = 0;

        settimeofday(&tv, &tz);

        timeSyncFlag = T_SYNC_APRS;

        TimeSyncPeriod = millis() + (60 * 60 * 1000); // 60 min
    }
}

uint8_t *packetData;

void aprs_msg_callback(struct AX25Msg *msg) {
    AX25Msg pkg;
    memcpy(&pkg, msg, sizeof(AX25Msg));
    // Prevent multiline messages
    size_t crlf_pos = pkg.len;
    for (int i = 0; i < pkg.len; i++) {
        if (pkg.info[i] == '\n' || pkg.info[i] == '\r') {
            crlf_pos = i;
            break;
        }
    }

    pkg.info[crlf_pos] = 0;
    pkg.len = crlf_pos;

    PacketBuffer.push(&pkg);

    status.rxCount++;
    
    aprsTimeGet(pkg.info);
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

static bool aprsCheckMsg(const char *from, const char *info) {
    if (!info || info[0] != ':' || strlen(info) < 11 || info[10] != ':') return false;

    char dest[10] = {0};
    strncpy(dest, info + 1, 9);
    for (int i = 8; i >= 0; i--) {
        if (dest[i] == ' ') dest[i] = '\0'; else break;
    }

    char ourCall[12];
    if (config.aprs_ssid > 0)
        snprintf(ourCall, sizeof(ourCall), "%s-%d", config.aprs_mycall, config.aprs_ssid);
    else
        strlcpy(ourCall, config.aprs_mycall, sizeof(ourCall));

    if (strcasecmp(dest, ourCall) != 0) return false;

    const char *body = info + 11;
    char msgText[70] = {0};
    char msgId[6]    = {0};

    const char *idOpen = strrchr(body, '{');
    if (idOpen) {
        const char *idClose = strchr(idOpen + 1, '}');
        int textLen = (int)(idOpen - body);
        if (textLen > 69) textLen = 69;
        strncpy(msgText, body, textLen);
        if (idClose) {
            int idLen = (int)(idClose - idOpen - 1);
            if (idLen > 5) idLen = 5;
            strncpy(msgId, idOpen + 1, idLen);
        }
    } else {
        strlcpy(msgText, body, sizeof(msgText));
    }

    aprsMsgAdd(from, msgText, msgId);
    log_i("[MSG] {\"event\":\"rx\",\"from\":\"%s\",\"text\":\"%s\"}", from, msgText);

    char fromBuf[12], textBuf[70];
    strlcpy(fromBuf, from,    sizeof(fromBuf));
    strlcpy(textBuf, msgText, sizeof(textBuf));
    if (config.aprs_sms_popup > 0)
        OledEnqueueMsg("MSG FROM", fromBuf, textBuf, config.aprs_sms_popup);

    if (msgId[0]) {
        char ackInfo[25];
        snprintf(ackInfo, sizeof(ackInfo), ":%-9s:ack%s", from, msgId);

        if (aprsClient.connected()) {
            String ackPkt = String(ourCall) + ">APRS:" + String(ackInfo);
            aprsClient.println(ackPkt);
            log_i("[MSG] {\"event\":\"ack_is\",\"pkt\":\"%s\"}", ackPkt.c_str());
        }

        if (callsignValid && config.tnc) {
            char rfAck[100];
            if (config.aprs_ssid > 0 && config.aprs_path[0] != 0)
                snprintf(rfAck, sizeof(rfAck), "%s-%d>%s,%s:%s", config.aprs_mycall, config.aprs_ssid, APRS_TOCALL, config.aprs_path, ackInfo);
            else if (config.aprs_ssid > 0)
                snprintf(rfAck, sizeof(rfAck), "%s-%d>%s:%s", config.aprs_mycall, config.aprs_ssid, APRS_TOCALL, ackInfo);
            else if (config.aprs_path[0] != 0)
                snprintf(rfAck, sizeof(rfAck), "%s>%s,%s:%s", config.aprs_mycall, APRS_TOCALL, config.aprs_path, ackInfo);
            else
                snprintf(rfAck, sizeof(rfAck), "%s>%s:%s", config.aprs_mycall, APRS_TOCALL, ackInfo);
            pkgTxPush(rfAck, strlen(rfAck), 0);
            log_i("[MSG] {\"event\":\"ack_rf\",\"pkt\":\"%s\"}", rfAck);
        }
    }
    return true;
}

boolean APRSConnect() {
    log_i("[APRS] {\"event\":\"connecting\"}");
    String login = "";
    uint8_t con = aprsClient.connected();
    if (con <= 0) {
        if (!callsignValid) return false;

        if (!aprsClient.connect(config.aprs_host, config.aprs_port)) {
            log_w("[APRS] {\"event\":\"connect_failed\"}");
            return false;
        }

        if (strlen(config.aprs_object) >= 3) {
            uint16_t passcode = aprsParse.passCode(config.aprs_object);
            login = "user " + String(config.aprs_object) + " pass " + String(passcode, DEC) + " vers APRS-ESP V" + String(VERSION_FULL) + " filter " + String(config.aprs_filter);
        } else {
            if (config.aprs_ssid == 0) {
                login = "user " + String(config.aprs_mycall) + " pass " + String(config.aprs_passcode) + " vers APRS-ESP V" + String(VERSION_FULL) + " filter " + String(config.aprs_filter);
            } else {
                login = "user " + String(config.aprs_mycall) + "-" + String(config.aprs_ssid) + " pass " + String(config.aprs_passcode) + " vers APRS-ESP V" + String(VERSION_FULL) + " filter " + String(config.aprs_filter);
            }
        }
        aprsClient.println(login);
        // Serial.println(login);
        log_i("[APRS] {\"event\":\"connected\"}");
        delay(500);
    }

    return true;
}

#if defined(USE_PMU)
bool vbusIn = false;
volatile bool pmu_flag = false;

void setFlag(void)
{
    pmu_flag = true;
}

void setupPower()
{
    bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, OLED_SDA_PIN, OLED_SCL_PIN);
    if (result == false) {
        while (1) {
            log_e("PMU is not online...");
            delay(500);
        }
    }

    log_i("PMU power-on  source: %u", (uint8_t)PMU.getPowerOnSource());
    log_i("PMU power-off source: %u", (uint8_t)PMU.getPowerOffSource());

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_3V88);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);


    // Get the VSYS shutdown voltage
    uint16_t vol = PMU.getSysPowerDownVoltage();
    log_i("->  getSysPowerDownVoltage:%u", vol);

    // 3000mV protects cell from deep discharge; 2600 is absolute spec minimum
    PMU.setSysPowerDownVoltage(3000);


    //! DC1 ESP32S3 Core VDD and OLED_VBAT , Don't change
    // PMU.setDC1Voltage(3400);        // +100mV // ESP32S3 can handle up to 3.6V, SH1106 up to 3.5V
    // PMU.settDC1WorkModeToPwm(1);    // manually switch Auto -> PWM mode
    PMU.setDC1Voltage(3300);

    //! DC3 Radio & Pixels VDD , Don't change
    PMU.setDC3Voltage(3400);

    //! ALDO2 MICRO TF Card VDD, Don't change
    PMU.setALDO2Voltage(3300);

    //! ALDO4 GNSS VDD, Don't change
    PMU.setALDO4Voltage(3300);

    //! BLDO1 MIC VDD, Don't change
    PMU.setBLDO1Voltage(3300);


    //! The following supply voltages can be controlled by the user
    // DC5 IMAX=2A
    // 1200mV
    // 1400~3700mV,100mV/step,24steps
    PMU.setDC5Voltage(3300);

    //ALDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO1Voltage(3300);

    //ALDO3 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO3Voltage(3300);

    //BLDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setBLDO2Voltage(3300);

    //! END


    // Turn on the power that needs to be used
    //! DC1 ESP32S3 Core VDD , Don't change
    // PMU.enableDC3();

    //! External pin power supply
    PMU.enableDC5();
    PMU.enableALDO1();
    PMU.enableALDO3();
    PMU.enableBLDO2();

    //! ALDO2 MICRO TF Card VDD
    PMU.enableALDO2();

    //! ALDO4 GNSS VDD
    PMU.enableALDO4();

    //! BLDO1 MIC VDD
    PMU.enableBLDO1();

    //! DC3 Radio & Pixels VDD
    PMU.enableDC3();

    // power off when not in use
    PMU.disableDC2();
    PMU.disableDC4();
    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();

    log_d("DCDC=======================================================================");
    log_d("DC1  : %s   Voltage:%u mV",  PMU.isEnableDC1()  ? "+" : "-", PMU.getDC1Voltage());
    log_d("DC2  : %s   Voltage:%u mV",  PMU.isEnableDC2()  ? "+" : "-", PMU.getDC2Voltage());
    log_d("DC3  : %s   Voltage:%u mV",  PMU.isEnableDC3()  ? "+" : "-", PMU.getDC3Voltage());
    log_d("DC4  : %s   Voltage:%u mV",  PMU.isEnableDC4()  ? "+" : "-", PMU.getDC4Voltage());
    log_d("DC5  : %s   Voltage:%u mV",  PMU.isEnableDC5()  ? "+" : "-", PMU.getDC5Voltage());
    log_d("ALDO=======================================================================");
    log_d("ALDO1: %s   Voltage:%u mV",  PMU.isEnableALDO1()  ? "+" : "-", PMU.getALDO1Voltage());
    log_d("ALDO2: %s   Voltage:%u mV",  PMU.isEnableALDO2()  ? "+" : "-", PMU.getALDO2Voltage());
    log_d("ALDO3: %s   Voltage:%u mV",  PMU.isEnableALDO3()  ? "+" : "-", PMU.getALDO3Voltage());
    log_d("ALDO4: %s   Voltage:%u mV",  PMU.isEnableALDO4()  ? "+" : "-", PMU.getALDO4Voltage());
    log_d("BLDO=======================================================================");
    log_d("BLDO1: %s   Voltage:%u mV",  PMU.isEnableBLDO1()  ? "+" : "-", PMU.getBLDO1Voltage());
    log_d("BLDO2: %s   Voltage:%u mV",  PMU.isEnableBLDO2()  ? "+" : "-", PMU.getBLDO2Voltage());
    log_d("===========================================================================");

    // Set the time of pressing the button to turn off
    PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU.getPowerKeyPressOffTime();
    String _str = "";
    switch (opt) {
    case XPOWERS_POWEROFF_4S: _str = "4 Second";
        break;
    case XPOWERS_POWEROFF_6S: _str = "6 Second";
        break;
    case XPOWERS_POWEROFF_8S: _str = "8 Second";
        break;
    case XPOWERS_POWEROFF_10S: _str = "10 Second";
        break;
    default:
        break;
    }
    log_d("PowerKeyPressOffTime:%s", _str.c_str());
    // Set the button power-on press time
    PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
    opt = PMU.getPowerKeyPressOnTime();
    switch (opt) {
    case XPOWERS_POWERON_128MS: _str = "128 Ms";
        break;
    case XPOWERS_POWERON_512MS: _str = "512 Ms";
        break;
    case XPOWERS_POWERON_1S: _str = "1 Second";
        break;
    case XPOWERS_POWERON_2S: _str = "2 Second";
        break;
    default:
        break;
    }
    log_d("PowerKeyPressOnTime:%s", _str.c_str());

    log_d("===========================================================================");
    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU.disableTSPinMeasure();


    // Enable internal ADC detection
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    PMU.enableTemperatureMeasure();  // required — without this getTemperature() returns stale power-on default (~38.4°C)
    PMU.enableGauge();               // enable Coulomb counter for load-compensated SoC%


    /*
      The default setting is CHGLED is automatically controlled by the PMU.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);

    // Force add pull-up
    pinMode(PMU_IRQ, INPUT_PULLUP);
    attachInterrupt(PMU_IRQ, setFlag, FALLING);

    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   // battery hot-swap
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   // USB connect/disconnect
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   // power key
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ   |   // charge phase transitions
        XPOWERS_AXP2101_CHAGER_TIMER_IRQ                                         |   // safety timer expiry (silent failure otherwise)
        XPOWERS_AXP2101_DIE_OVER_TEMP_IRQ                                        |   // PMU chip overtemperature
        XPOWERS_AXP2101_BAT_OVER_VOL_IRQ                                         |   // battery overvoltage (safety)
        XPOWERS_AXP2101_BAT_CHG_OVER_TEMP_IRQ                                    |   // charger over-temp (future-proof; inert with TS disabled)
        XPOWERS_AXP2101_WARNING_LEVEL1_IRQ | XPOWERS_AXP2101_WARNING_LEVEL2_IRQ      // hardware low-battery thresholds
    );

    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_150MA);

    // Set constant current charge current limit
    //! Using inferior USB cables and adapters will not reach the maximum charging current.
    //! Please pay attention to add a suitable heat sink above the PMU when setting the charging current to 1A
    // 500mA: CC(500) + system load(~300) = ~800mA total from VBUS — safe for 1A adapters.
    // 1000mA caused VBUS sag on 1A adapters (1300mA demanded), triggering the 3.88V cutoff
    // and stalling charge at ~90% in CV phase. 2A adapters were unaffected.
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);

    // 25mA (minimum): prevents premature CHG_DONE under load — 150mA was firing at ~85% because
    // device load (WiFi+RF 200-400mA) consumed most of the charge current during CV phase
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    // Thermal foldback threshold for die temperature: 80°C (chip default, set explicitly)
    PMU.setThermaThreshold(XPOWERS_AXP2101_THREMAL_80DEG);
    // Arm the hardware die temperature comparator (required for DIE_OVER_TEMP_IRQ to fire)
    PMU.enableDieOverTempDetect();
    // Hardware low-battery thresholds (fuel gauge must be enabled): 15% warn, 5% critical
    PMU.setLowBatWarnThreshold(2);      // 2 = 15%
    PMU.setLowBatShutdownThreshold(0);  // 0 = 5%

    // Disable the PMU long press shutdown function (handled in firmware via PKEY_LONG_IRQ)
    PMU.disableLongPressShutdown();


    // Get charging target current
    const uint16_t currTable[] = {
        0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000
    };
    uint8_t val = PMU.getChargerConstantCurr();
    log_d("Val = %u", val);
    log_d("Setting Charge Target Current : %u", currTable[val]);

    // Get charging target voltage
    const uint16_t tableVoltage[] = {
        0, 4000, 4100, 4200, 4350, 4400, 255
    };
    val = PMU.getChargeTargetVoltage();
    log_d("Setting Charge Target Voltage : %u", tableVoltage[val]);

}
#endif

// #include "soc/soc.h"
// #include "soc/rtc_cntl_reg.h"

void setup()
{
    // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

#ifdef BOARD_HAS_PSRAM
    pkgListInit();
    Telemetry = (TelemetryType *)malloc(sizeof(TelemetryType) * TLMLISTSIZE);
    txQueue = (txQueueType *)ps_malloc(sizeof(txQueueType) * PKGTXSIZE);
    // TNC2Raw = (int *)ps_malloc(sizeof(int) * PKGTXSIZE);
#else
    pkgListInit();
    Telemetry = (TelemetryType *)malloc(sizeof(TelemetryType) * TLMLISTSIZE);
    txQueue = (txQueueType *)malloc(sizeof(txQueueType) * PKGTXSIZE);
    // TNC2Raw = (int *)malloc(sizeof(int) * PKGTXSIZE);
#endif

    memset(Telemetry, 0, sizeof(TelemetryType) * TLMLISTSIZE);
    memset(txQueue, 0, sizeof(txQueueType) * PKGTXSIZE);
    // memset(TNC2Raw, 0, sizeof(TNC2Raw) * PKGTXSIZE);

    psramMutex = xSemaphoreCreateMutex();

    pinMode(BOOT_PIN, INPUT_PULLUP);  // BOOT Button

#if defined(USER_BUTTON)
    pinMode(USER_BUTTON, INPUT_PULLUP);  // USER Button
#endif

    // Set up serial port
    Serial.setRxBufferSize(256);
    Serial.begin(SERIAL_DEBUG_BAUD);  // debug

#if defined(USE_GPS)
    SerialGPS.setRxBufferSize(1024);
    SerialGPS.begin(SERIAL_GPS_BAUD, SERIAL_8N1, SERIAL_GPS_RXPIN, SERIAL_GPS_TXPIN);
#endif


#ifdef USE_ROTARY
    rotary.begin();
    attachInterrupt(digitalPinToInterrupt(PIN_ROT_CLK), rotaryISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ROT_DT),  rotaryISR, CHANGE);
#else
    pinMode(PIN_ROT_BTN, INPUT_PULLUP);
#endif

    TX_LED_OFF();
    RX_LED_OFF();

    if (!LittleFS.begin(false)) {
        log_e("LittleFS mount failed — reflash filesystem image");
    }

    Serial.println();
    log_i("Start APRS-ESP V%s", VERSION_FULL);
    log_i("Press and hold BOOT button for 3 sec to Factory Default config");

#if defined(USE_SCREEN) || defined(USE_PMU)
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN, 400000L);
#endif


#if defined(BOARD_TTWR_V1)
    // +4.2V EN
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);

    // OLED EN
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
#endif

#if defined(ADC_BATTERY)
    // Battery Voltage
    pinMode(ADC_BATTERY, INPUT);
#endif

#if defined(USE_PMU)
    setupPower();
#endif

#ifdef USE_SCREEN
    OledStartup();
#endif

#if defined(BOARD_TTWR_PLUS)
    // MIC Select
    pinMode(MIC_CH_SEL, OUTPUT);
    digitalWrite(MIC_CH_SEL, HIGH);  // LOW - MIC / HIGH - ESP32

    // NeoPixel
    strip.setBrightness(100);
    strip.begin();

    strip.setPixelColor(0, 0, 0, 255);  // Blue
    strip.show();
#endif

    OledPostStartup("Loading config...");
    LoadConfig();
    OledSetBrightness(config.oled_brightness);
    OledPostStartup("Loading config... OK");

    // RF SHOULD BE Initialized or there is no reason to startup at all
    while (!RF_Init(true, true)) {};

#if defined(USE_PMU)
    if (PMU.isVbusIn()) {
        vbusIn = true;
        log_i("Vbus In");
        // PMU.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);
    }
#endif

    // if SmartBeaconing - delay processing GPS data
    if (config.aprs_beacon == 0) {
        log_i("SmartBeaconing, delayed GPS processing");
        gpsUpdTMO = 30000;  // 30 sec
    }

    // enableLoopWDT();
    // enableCore0WDT();

    enableCore1WDT();

#if defined(BOARD_TTWR_PLUS) && defined(USE_GPS)
    {
        auto gpsSendCmd = [](const char *cmd) {
            uint8_t cs = 0;
            for (const char *p = cmd; *p; p++) cs ^= (uint8_t)*p;
            SerialGPS.printf("$%s*%02X\r\n", cmd, cs);
            log_i("GNSS TX: $%s*%02X", cmd, cs);
        };
        gpsSendCmd("PCAS04,7");                                // Constellation: GPS+BeiDou+GLONASS (default GPS+BeiDou only)
        gpsSendCmd("PCAS03,1,0,1,0,1,0,0,0,0,0,,,0,0");       // NMEA output: GGA+GSA+RMC, disable GLL/GSV/VTG/ZDA/ANT
        gpsSendCmd("PCAS02,1000");                             // Fix rate: 1 Hz
    }
#endif

    // Task 1
    xTaskCreatePinnedToCore(taskAPRS,   /* Function to implement the task */
                            "taskAPRS", /* Name of the task */
                            16384,      /* Stack size in words */
                            NULL,       /* Task input parameter */
#if defined(BOARD_ESP32DR)
                            1,          /* Priority of the task */
#else
                            3,          /* Priority of the task */
#endif
                            &taskAPRSHandle, /* Task handle. */
                            0); /* Core where the task should run */

    // Task 2
    xTaskCreatePinnedToCore(taskNetwork,   /* Function to implement the task */
                            "taskNetwork", /* Name of the task */
                            (32768),       /* Stack size in words */
                            NULL,          /* Task input parameter */
#if defined(BOARD_ESP32DR)
                            1,             /* Priority of the task */
#else
                            2,             /* Priority of the task */
#endif
                            &taskNetworkHandle, /* Task handle. */
                            1); /* Core where the task should run */

    // Task 3
    xTaskCreatePinnedToCore(taskOLEDDisplay,        /* Function to implement the task */
                            "taskOLEDDisplay",      /* Name of the task */
                            8192,                   /* Stack size in words */
                            NULL,                   /* Task input parameter */
                            1,                      /* Priority of the task */
                            &taskOLEDDisplayHandle, /* Task handle. */
                            1);                     /* Core where the task should run */
    
    // Task 4
    xTaskCreatePinnedToCore(taskGPS,                /* Function to implement the task */
                            "taskGPS",              /* Name of the task */
                            4096,                   /* Stack size in words */
                            NULL,                   /* Task input parameter */
                            2,                      /* Priority of the task */
                            &taskGPSHandle,         /* Task handle. */
                            0);                     /* Core where the task should run */
    
#if !defined(BOARD_ESP32DR)
    // Task 5
    xTaskCreatePinnedToCore(taskTNC,                /* Function to implement the task */
                            "taskTNC",              /* Name of the task */
                            16384,                  /* Stack size in words */
                            NULL,                   /* Task input parameter */
                            1,                      /* Priority of the task */
                            &taskTNCHandle,         /* Task handle. */
                            0);                     /* Core where the task should run */
#endif
}

int pkgCount = 0;

String send_gps_location() {
    String tnc2Raw = "";
    
    float _lat = 0.0;
    float _lon = 0.0;

    if (config.gps_mode == GPS_MODE_AUTO) {
        log_i("GPS Mode: Auto");
        //if (/*age != (uint32_t)ULONG_MAX &&*/ gps.location.isValid() /*|| gotGpsFix*/) {
        if (gps.location.isValid()) {
            _lat = lat / 1000000.0;
            _lon = lon / 1000000.0;
            distance = 0;   // Reset counted distance
            log_i("GPS Fix, using current location");
        } else {
            _lat = config.gps_lat;
            _lon = config.gps_lon;
            log_i("No GPS Fix, using fixed location");
        }
    } else if (config.gps_mode == GPS_MODE_FIXED) {
        log_i("GPS Mode: Fixed Only");
        _lat = config.gps_lat;
        _lon = config.gps_lon;
    } else if (config.gps_mode == GPS_MODE_GPS) {
        log_i("GPS Mode: GPS Only");
        if (gps.location.isValid()) {
            _lat = lat / 1000000.0;
            _lon = lon / 1000000.0;
            distance = 0;   // Reset counted distance
            log_i("GPS Fix, using current location");
        } else {
            log_i("No GPS Fix, skipped");
            return "";
        }
    }

    if (_lat == 0.0f && _lon == 0.0f) {
        log_w("Zero coordinates, skipping TX");
        return "";
    }

    int lat_dd, lat_mm, lat_ss, lon_dd, lon_mm, lon_ss;
    char strtmp[300], loc[30];

    memset(strtmp, 0, 300);
    char lon_ew = 'E';
    char lat_ns = 'N';

    DD_DDDDDtoDDMMSS(_lat, &lat_dd, &lat_mm, &lat_ss);
    DD_DDDDDtoDDMMSS(_lon, &lon_dd, &lon_mm, &lon_ss);

    if (_lat < 0) {
        lat_ns = 'S';
    }
    if (_lon < 0) {
        lon_ew = 'W';
    }

    // sprintf(loc, "=%02d%02d.%02dN%c%03d%02d.%02dE%c", 
    //         lat_dd, lat_mm, lat_ss, config.aprs_table, lon_dd, lon_mm, lon_ss, config.aprs_symbol);
    
    if (strlen(config.aprs_object) >= 3) {
        sprintf(loc, ")%s!%02d%02d.%02d%c%c%03d%02d.%02d%c%c",
            config.aprs_object, lat_dd, lat_mm, lat_ss, lat_ns, config.aprs_table, lon_dd, lon_mm, lon_ss, lon_ew, config.aprs_symbol);
    } else {
        sprintf(loc, "!%02d%02d.%02d%c%c%03d%02d.%02d%c%c",
            lat_dd, lat_mm, lat_ss, lat_ns, config.aprs_table, lon_dd, lon_mm, lon_ss, lon_ew, config.aprs_symbol);
    }

    if (config.aprs_ssid == 0)
        sprintf(strtmp, "%s>%s", config.aprs_mycall, APRS_TOCALL);
    else
        sprintf(strtmp, "%s-%d>%s", config.aprs_mycall, config.aprs_ssid, APRS_TOCALL);

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

    return tnc2.length();
}

#if defined(USE_PMU)
uint8_t getBatteryPercentage() {
// #if BATTERY_ADC_PIN != -1
//     esp_adc_cal_characteristics_t adc_chars;
//     esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
//     uint32_t v = esp_adc_cal_raw_to_voltage(analogRead(BATTERY_ADC_PIN), &adc_chars);
//     float battery_voltage = ((float)v / 1000) * 2;
//     Serial.print("ADC RAW: ");
//     Serial.println(v);
//     Serial.print("Battery voltage: ");
//     Serial.println(battery_voltage);
//     if (battery_voltage > 4.2) {
//         return 100;
//     } else {
//         return (uint8_t)(((battery_voltage - 3.0) / (4.2 - 3.0)) * 100);
//     }
// #else
    return PMU.isBatteryConnect() ? PMU.getBatteryPercent() : 0;
// #endif
}
#endif

#if defined(ADC_BATTERY)
static uint16_t batteryVoltage = 0;
#endif
#if defined(ADC_BATTERY) || defined(USE_PMU)
static uint8_t batteryPercentage = 0;
#endif

#if defined(ADC_BATTERY)
#include <esp_adc_cal.h>
uint8_t getBatteryPercentage() {
    esp_adc_cal_characteristics_t adc_chars;
    // esp_adc_cal_value_t val_type = 
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    uint32_t v = esp_adc_cal_raw_to_voltage(analogRead(ADC_BATTERY), &adc_chars);
    v += (v > 0 ? (BATT_OFFSET / 2) : 0);
    float battery_voltage = ((float)v / 1000) * 2;
    // Serial.print("ADC RAW: ");
    // Serial.println(v);
    // Serial.print("Battery voltage: ");
    // Serial.println(battery_voltage);
    batteryVoltage = battery_voltage * 1000;
    if (battery_voltage > 4.2) {
        return 100;
    } else if (battery_voltage < 3.0) {
        return 0;
    } else {
        return (uint8_t)(((battery_voltage - 3.0) / (4.2 - 3.0)) * 100);
    }
}
#endif

void dbgTick() {
#if defined(ADC_BATTERY)
    if (config.wifi_mode == WIFI_OFF) {
        batteryPercentage = getBatteryPercentage();
    } else {
        batteryPercentage = digitalRead(ADC_BATTERY);
    }
#endif
#if defined(USE_PMU)
    batteryPercentage = getBatteryPercentage();
    if (!vbusIn && PMU.isBatteryConnect()) {
        static unsigned long lastBatWarnMs = 0;
        static uint8_t lastBatThreshold = 100;
        if (batteryPercentage < 20 && millis() - lastBatWarnMs >= 120000UL) {
            lastBatWarnMs = millis();
            char cap[] = "Battery";
            char msg[] = "Low!";
            char empty[] = "";
            OledPushMsg(cap, msg, empty, 2);
        }
        static const uint8_t thresholds[] = {50, 40, 30, 20, 10};
        for (uint8_t t : thresholds) {
            if (batteryPercentage <= t && lastBatThreshold > t) {
                lastBatThreshold = t;
                char cap[] = "Battery";
                char pctMsg[6];
                snprintf(pctMsg, sizeof(pctMsg), "%d%%", t);
                char empty[] = "";
                OledPushMsg(cap, pctMsg, empty, 2);
                break;
            }
        }
        if (batteryPercentage > 50) lastBatThreshold = 100;
    }
#endif

    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    bool fix = gps.location.isValid();
    int32_t latI = fix ? (int32_t)(gps.location.lat() * 1e6) : 0;
    int32_t lonI = fix ? (int32_t)(gps.location.lng() * 1e6) : 0;
    uint32_t ageS = fix ? (uint32_t)(gps.location.age() / 1000) : 0;

    char strtmp[200];
#if defined(ADC_BATTERY)
    snprintf(strtmp, sizeof(strtmp),
        "[DEBUG] {\"t\":\"%02d:%02d:%02d\",\"fix\":%d,\"gps\":%d,\"lat\":%ld,\"lon\":%ld,\"age\":%u,\"dist\":%d,\"bat\":%d,\"bat_mv\":%d}",
        tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec,
        fix ? 1 : 0, GpsPktCnt() > 0 ? 1 : 0,
        (long)latI, (long)lonI, ageS, distance, batteryPercentage, batteryVoltage);
#elif defined(USE_PMU)
    snprintf(strtmp, sizeof(strtmp),
        "[DEBUG] {\"t\":\"%02d:%02d:%02d\",\"fix\":%d,\"gps\":%d,\"lat\":%ld,\"lon\":%ld,\"age\":%u,\"dist\":%d,\"bat\":%d}",
        tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec,
        fix ? 1 : 0, GpsPktCnt() > 0 ? 1 : 0,
        (long)latI, (long)lonI, ageS, distance, batteryPercentage);
#else
    snprintf(strtmp, sizeof(strtmp),
        "[DEBUG] {\"t\":\"%02d:%02d:%02d\",\"fix\":%d,\"gps\":%d,\"lat\":%ld,\"lon\":%ld,\"age\":%u,\"dist\":%d}",
        tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec,
        fix ? 1 : 0, GpsPktCnt() > 0 ? 1 : 0,
        (long)latI, (long)lonI, ageS, distance);
#endif
    log_i("%s", strtmp);
}

bool gpsUnlock = false;

#if defined(USE_PMU)
void loopPMU()
{
    if (!pmu_flag) {
        return;
    }

    pmu_flag = false;
    uint32_t status = PMU.getIrqStatus();
    log_i("PMU IRQ => HEX:%x", status);

    // --- Power key short: battery status popup ---
    if (PMU.isPekeyShortPressIrq()) {
        log_i("BTN SW3/PWRON SHORT");
        if (OledIsPopupActive()) {
            OledDismissPopup();
        } else {
            uint8_t pct = getBatteryPercentage();
            char batVolt[8], batPct[16];
            snprintf(batVolt, sizeof(batVolt), "%.2fV", PMU.getBattVoltage() / 1000.0f);
            const char *suffix;
            switch (PMU.getChargerStatus()) {
                case XPOWERS_AXP2101_CHG_TRI_STATE:  suffix = " TRICKLE"; break;
                case XPOWERS_AXP2101_CHG_PRE_STATE:  suffix = " PRE-CHG"; break;
                case XPOWERS_AXP2101_CHG_CC_STATE:   suffix = " CHG CC";  break;
                case XPOWERS_AXP2101_CHG_CV_STATE:   suffix = " CHG CV";  break;
                case XPOWERS_AXP2101_CHG_DONE_STATE: suffix = " FULL";    break;
                default: suffix = vbusIn ? " USB" : "";                   break;
            }
            snprintf(batPct, sizeof(batPct), "%d%%%s", pct, suffix);
            OledPushMsgBar("Battery", batVolt, batPct, (int8_t)pct, 2);
        }
    }

    // --- Battery inserted ---
    if (PMU.isBatInsertIrq()) {
        log_i("Battery Inserted");
        OledPushMsgDual("Battery", "Inserted", 2);
        if (PMU.isCharging()) PMU.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);
    }

    // --- Charge started ---
    if (PMU.isBatChagerStartIrq()) {
        log_i("Charge Start");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);
        OledPushMsgDual("Charge", "Charging", 2);
    }

    // --- Charge complete ---
    if (PMU.isBatChagerDoneIrq()) {
        log_i("Charge Done");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);  // steady = full; BLINK_1HZ was indistinguishable from "no USB"
        OledPushMsgDual("Charge", "Full!", 2);
    }

    // --- Charge safety timer expired (would be silent without this IRQ) ---
    if (PMU.isChagerOverTimeoutIrq()) {
        log_w("Charge safety timer expired");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
        OledPushMsgDual("Charge", "Timeout!", 3);
    }

    // --- Battery removed ---
    if (PMU.isBatRemoveIrq()) {
        log_i("Battery Removed");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
        OledPushMsgDual("Battery", "Removed", 2);
    }

    // --- USB connected ---
    if (PMU.isVbusInsertIrq()) {
        log_i("USB Connected");
        vbusIn = true;
        OledPushMsgDual("USB", "Connected", 1);
    }

    // --- USB removed: reset LED since charging stops ---
    if (PMU.isVbusRemoveIrq()) {
        log_i("USB Removed");
        vbusIn = false;
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
        OledPushMsgDual("USB", "Removed", 1);
    }

    // --- PMU die overtemperature: charger throttles/stops ---
    if (PMU.isBatDieOverTemperatureIrq()) {
        log_e("PMU die over-temperature");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
        OledPushMsgDual("PMU", "Overtemp!", 4);
    }

    // --- Charger over-temperature (TS pin; inert with TS disabled, future-proof) ---
    if (PMU.isBatChargerOverTemperatureIrq()) {
        log_e("Charger over-temperature");
        OledPushMsgDual("Charge", "Overtemp!", 3);
    }

    // --- Battery overvoltage (hardware safety event) ---
    if (PMU.isBatOverVoltageIrq()) {
        log_e("Battery overvoltage!");
        OledPushMsgDual("Battery", "Overvolt!", 4);
    }

    // --- Hardware low-battery warnings (fuel gauge threshold crossing) ---
    if (PMU.isDropWarningLevel1Irq()) {
        log_w("Battery low: 15%%");
        char cap[] = "Battery"; char msg[] = "Low 15%!"; char empty[] = "";
        OledPushMsg(cap, msg, empty, 3);
    }
    if (PMU.isDropWarningLevel2Irq()) {
        log_e("Battery critical: 5%%");
        char cap[] = "Battery"; char msg[] = "Critical!"; char empty[] = "";
        OledPushMsg(cap, msg, empty, 4);
    }

    // --- Power key long press: shutdown ---
    if (PMU.isPekeyLongPressIrq()) {
        log_i("Long press — shutdown");
        PMU.shutdown();
    }

    PMU.clearIrqStatus();
}
#endif

long sendTimer = 0;
volatile unsigned long lastTxMs = 0;  // millis() of last actual beacon TX; 0 = never
#ifndef USE_ROTARY
int btn_count = 0;
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
const int btnCnt = 500;
#else
const int btnCnt = 1000;
#endif
#endif  // USE_ROTARY
long timeCheck = 0;
long TimeSyncPeriod = 0;

void loop()
{
#if defined(BOARD_ESP32DR)
    vTaskDelay(5 / portTICK_PERIOD_MS);
#else
    vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
    if (!fwUpdateProcess) {
        if (millis() > timeCheck) {
            timeCheck = millis() + 10000;
            if (ESP.getFreeHeap() < 60000) esp_restart();
            // Serial.println(String(ESP.getFreeHeap()));
        }

        if (millis() > TimeSyncPeriod) {
            TimeSyncPeriod = millis() + (60 * 60 * 1000); // 60 min
            if (timeSyncFlag != T_SYNC_NONE && WiFi.status() != WL_CONNECTED) {
                // Reset time sync flag
                timeSyncFlag = T_SYNC_NONE;
                log_i("TimeSync Flag Reset");
            }
        }
    } // if (!fwUpdateProcess)
        
    if (RotaryProcess()) menuUpdateNeeded = true;

    if (fwUpdateProcess) return;

#ifndef USE_ROTARY
    uint8_t bootPin2 = digitalRead(PIN_ROT_BTN);
    String _empty = "";

    int rot_sw = digitalRead(BOOT_PIN);

    if (rot_sw == LOW || bootPin2 == LOW) {
        btn_count++;
        if (btn_count > btnCnt && btn_count < btnCnt * 2)  // Push BOOT 5sec
        {
            RX_LED_ON();
            TX_LED_ON();
            String _msg = "WiFi SW";
            OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
        }
        if (btn_count > btnCnt * 2 && btn_count < btnCnt * 3)  // Push BOOT 10sec
        {
            RX_LED_OFF();
            TX_LED_OFF();
            String _msg = "RF SW";
            OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
        }
        if (btn_count > btnCnt * 3 && btn_count < btnCnt * 4)  // Push BOOT 15sec
        {
            RX_LED_OFF();
            TX_LED_OFF();
            String _msg = "SB SW";
            OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
        }
        if (btn_count > btnCnt * 4 && btn_count < btnCnt * 5)  // Push BOOT 20sec
        {
            RX_LED_ON();
            TX_LED_ON();
            String _msg = "Reset";
            OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
        }
        if (btn_count >= btnCnt * 5)  // Push BOOT 25sec+
        {
            RX_LED_OFF();
            TX_LED_OFF();
            String _msg = "Cancel";
            OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
        }
        if (btn_count > btnCnt * 6) btn_count = btnCnt * 6;  // clamp, don't wrap
    } else {
        if (btn_count > 0) {
            // Serial.printf("btn_count=%dms\n", btn_count * 10);
            if (btn_count > btnCnt && btn_count < btnCnt * 2)
            {
                if (config.wifi_mode == WIFI_OFF) {
                    config.wifi_mode = WIFI_AP_STA_FIX;
                    log_i("WiFi ON");
                    String _msg = "WiFi ON";
                    OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
                    OledUpdate(0, false, false);
                    delay(2000);
                } else {
                    config.wifi_mode = WIFI_OFF;
                    log_i("WiFi OFF");
                    String _msg = "WiFi OFF";
                    OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
                    OledUpdate(0, false, false);
                    delay(2000);
                }
                SaveConfig();
                esp_restart();
            }
            else if (btn_count > btnCnt * 2 && btn_count < btnCnt * 3)
            {
                if (config.tnc == true) {
                    config.tnc = false;
                    log_i("RF OFF");
                    String _msg = "RF OFF";
                    OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 3);
                    OledUpdate(0, false, false);
                    delay(2000);
                } else {
                    String _msg = "";
                    if (callsignValid) {
                        log_i("RF ON");
                        _msg = "RF ON";
                        config.tnc = true;
                    } else {
                        log_w("Invalid callsign!");
                        _msg = "Invalid";
                        config.tnc = false;
                    }
                    OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 3);
                    OledUpdate(0, false, false);
                    delay(2000);
                }
                SaveConfig();
            }
            else if (btn_count > btnCnt * 3 && btn_count < btnCnt * 4)
            {
                static int bootBeaconLastFixed = 10;
                if (config.aprs_beacon == 0) {
                    config.aprs_beacon = bootBeaconLastFixed > 0 ? bootBeaconLastFixed : 10;
                } else {
                    bootBeaconLastFixed = config.aprs_beacon;
                    config.aprs_beacon = 0;
                }
                SaveConfig();
                sendTimer = millis();
                char msgSB[]  = "SB ON";
                char msgFix[] = "SB OFF";
                OledPushMsg("", config.aprs_beacon == 0 ? msgSB : msgFix, (char *)_empty.c_str(), 3);
                OledUpdate(0, false, false);
            }
            else if (btn_count > btnCnt * 4 && btn_count < btnCnt * 5)
            {
                log_i("Factory Reset");
                String _msg = "Reset OK";
                OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 15);
                OledUpdate(0, false, false);
                delay(2000);
                DefaultConfig();
                SaveConfig();
                esp_restart();
            }
            else if (btn_count >= btnCnt * 5)
            {
                log_i("Cancel");
                String _msg = "Cancelled";
                OledPushMsg("", (char *)_msg.c_str(), (char *)_empty.c_str(), 3);
                OledUpdate(0, false, false);
                // delay(2000);
            }
            else if (btn_count > 10)  // Push BOOT >100mS to PTT Fix location
            {
                if (callsignValid && config.tnc) {
                    String tnc2Raw = send_gps_location();
                    if (tnc2Raw.length() > 0) {
                        pkgTxPush(tnc2Raw.c_str(), tnc2Raw.length(), 0);
                        // APRS_sendTNC2Pkt(tnc2Raw); // Send packet to RF
                        log_i("Manual TX: %s", tnc2Raw.c_str());
                    }
                }
            }
            btn_count = 0;
        }
    }
#endif  // USE_ROTARY

#if defined(BOARD_TTWR_PLUS)
    {
        static uint32_t sw4Ms = 0, sw1Ms = 0;
        static bool sw4Prev = HIGH, sw1Prev = HIGH;
        bool sw4 = digitalRead(USER_BUTTON);
        bool sw1 = digitalRead(BOOT_PIN);

        if (sw4 == LOW && sw4Prev == HIGH) sw4Ms = millis();
        if (sw4 == LOW && sw4Ms && (millis() - sw4Ms >= 2000)) {
            log_i("[BTN] {\"btn\":\"SW4\",\"type\":\"long\",\"action\":\"rf_toggle\"}");
            sw4Ms = 0;
            config.tnc = !config.tnc;
            SaveConfig();
            char msgOn[] = "RF ON";  char msgOff[] = "RF OFF";  char empty[] = "";
            OledPushMsg("", config.tnc ? msgOn : msgOff, empty, 3);
        }
        if (sw4 == HIGH && sw4Prev == LOW && sw4Ms) {
            uint32_t held = millis() - sw4Ms;  sw4Ms = 0;
            if (held >= 50) {
                log_i("[BTN] {\"btn\":\"SW4\",\"type\":\"short\",\"held_ms\":%u,\"action\":\"beacon\"}", held);
                if (OledIsPopupActive()) {
                    OledDismissPopup();
                } else {
                    char empty[] = "";
                    if (!config.tnc) {
                        char msg[] = "RF IS OFF";  OledPushMsg("", msg, empty, 3);
                    } else if (config.gps_mode == GPS_MODE_GPS && !gps.location.isValid()) {
                        char msg[] = "NO GPS FIX";  OledPushMsg("", msg, empty, 3);
                    } else if (lastTxMs > 0 && millis() - lastTxMs < 30000UL) {
                        char waitMsg[16], tooFast[] = "Too fast!";
                        snprintf(waitMsg, sizeof(waitMsg), "Wait %ds!", (int)((30000UL - (millis() - lastTxMs) + 999) / 1000));
                        OledPushMsg("", tooFast, waitMsg, 3);
                    } else {
                        menuMode = MENU_HIDDEN;  // must precede flag: OledPushMsg I2C takes ~25ms during which taskAPRS reads the flag
                        send_aprs_update = true;
                        char msg[] = "Beacon TX";  OledPushMsg("", msg, empty, 1);
                    }
                }
            }
        }
        sw4Prev = sw4;

        if (sw1 == LOW && sw1Prev == HIGH) sw1Ms = millis();
        if (sw1 == LOW && sw1Ms && (millis() - sw1Ms >= 2000)) {
            log_i("[BTN] {\"btn\":\"SW1\",\"type\":\"long\",\"action\":\"rf_power_toggle\"}");
            sw1Ms = 0;
            config.rf_power = !config.rf_power;
            SaveConfig();
#ifdef POWER_PIN
            digitalWrite(POWER_PIN, config.rf_power);
#endif
            char msgHi[] = "RF High";  char msgLo[] = "RF Low";  char empty[] = "";
            OledPushMsg("", config.rf_power ? msgHi : msgLo, empty, 3);
        }
        if (sw1 == HIGH && sw1Prev == LOW && sw1Ms) {
            uint32_t held = millis() - sw1Ms;  sw1Ms = 0;
            if (held >= 50) {
                log_i("[BTN] {\"btn\":\"SW1\",\"type\":\"short\",\"held_ms\":%u,\"action\":\"display_toggle\"}", held);
                if (OledIsPopupActive()) {
                    OledDismissPopup();
                } else {
                    OledResetActivity();
                    OledTogglePower();
                }
            }
        }
        sw1Prev = sw1;
    }
#endif

#if defined(BOARD_ESP32DR)
    if (AFSKInitAct == true) {
        AFSK_Poll(true);
    }
#endif
}


void sendIsPkg(char *raw) {
    char str[300];
    sprintf(str, "%s-%d>%s%s:%s", config.aprs_mycall, config.aprs_ssid, APRS_TOCALL, VERSION, raw);
    // client.println(str);
    String tnc2Raw = String(str);
    if (callsignValid && aprsClient.connected())
        aprsClient.println(tnc2Raw); // Send packet to Inet
    if (callsignValid && config.tnc && config.tnc_digi)
        pkgTxPush(str, strlen(str), 0);
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
    for (; i < 9; i++)
        call[i] = 0x20;

    if (config.aprs_ssid == 0)
        sprintf(str, "%s>%s::%s:%s", config.aprs_mycall, APRS_TOCALL, call, raw);
    else
        sprintf(str, "%s-%d>%s::%s:%s", config.aprs_mycall, config.aprs_ssid, APRS_TOCALL, call, raw);

    String tnc2Raw = String(str);
    if (callsignValid && aprsClient.connected())
        aprsClient.println(tnc2Raw); // Send packet to Inet
    if (callsignValid && config.tnc && config.tnc_digi)
        pkgTxPush(str, strlen(str), 0);
    
    // APRS_sendTNC2Pkt(tnc2Raw); // Send packet to RF
}

long timeSlot;

void taskAPRS(void *pvParameters) {
    //	long start, stop;
    // char *raw;
    // char *str;
    log_i("Task <APRS> started");
    PacketBuffer.clean();

    APRS_init(config.rx_att);
    APRS_setCallsign(config.aprs_mycall, config.aprs_ssid);
    APRS_setPath1(config.aprs_path, 1);
    APRS_setPreamble(APRS_PREAMBLE);
    APRS_setTail(APRS_TAIL);

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
            // Transmit in timeslot if enabled
            if (SQL_PIN > -1) { // Set SQL pin
                if (!digitalRead(SQL_PIN)) { // RX State Fail
                    if (pkgTxSend())
                        timeSlot = millis() + config.tx_timeslot; // Tx Time Slot >2sec.
                    else
                        timeSlot = millis() + 3000;
                } else {
                    timeSlot = millis() + 1500;
                }
            } else {
                if (pkgTxSend())
                    timeSlot = millis() + config.tx_timeslot; // Tx Time Slot > 2sec.
                else
                    timeSlot = millis() + 3000;
            }
        }

        bool sendByTime = false;
        if (config.aprs_beacon > 0) {   // it interval configured
            sendByTime = (now > (sendTimer + (config.aprs_beacon * 60 * 1000)));
        }
        bool sendByFlag = send_aprs_update;

        send_aprs_update = false;

        if ((sendByTime || sendByFlag) && menuMode == MENU_HIDDEN) {
            sendTimer = now;
            lastTxMs  = now;

            if (sendByTime) log_i("[TX] {\"event\":\"send\",\"trigger\":\"time\"}");
            if (sendByFlag) log_i("[TX] {\"event\":\"send\",\"trigger\":\"flag\"}");

            if (digiCount > 0) digiCount--;

            RF_Check();

            if (AFSKInitAct == true) {
                if (callsignValid && config.tnc) {
                    String rawData = send_gps_location();
                    if (rawData.length() > 0) {
                        if (aprsClient.connected()) {
                            aprsClient.println(rawData);  // Send packet to Inet
                        }
                        
                        // char *rawP = (char *)malloc(rawData.length());
                        // memcpy(rawP, rawData.c_str(), rawData.length());
                        // // rawData.toCharArray(rawP, rawData.length());
                        // pkgTxPush(rawP, rawData.length(), 0);
                        // free(rawP);

                        pkgTxPush(rawData.c_str(), rawData.length(), 0);
                    }
                }
            }
        }

        if (callsignValid && config.tnc_telemetry) {
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
                    sprintf(rawTlm, "%s>%s:T#%03d,%d,%d,%d,%d,%d,00000000",
                            config.aprs_mycall, APRS_TOCALL, igateTLM.Sequence,
                            igateTLM.RF2INET, igateTLM.INET2RF, igateTLM.RX,
                            igateTLM.TX, igateTLM.DROP);
                } else {
                    sprintf(
                        rawTlm, "%s-%d>%s:T#%03d,%d,%d,%d,%d,%d,00000000",
                        config.aprs_mycall, config.aprs_ssid, APRS_TOCALL, igateTLM.Sequence,
                        igateTLM.RF2INET, igateTLM.INET2RF, igateTLM.RX,
                        igateTLM.TX, igateTLM.DROP);
                }

                if (callsignValid && aprsClient.connected()) {
                    aprsClient.println(String(rawTlm));  // Send packet to Inet
                }
                if (callsignValid && config.tnc && config.tnc_digi) {
                    pkgTxPush(rawTlm, strlen(rawTlm), 0);
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
        if (callsignValid && config.tnc) {
            if (PacketBuffer.getCount() > 0) {
                String tnc2;
                PacketBuffer.pop(&incomingPacket);
                // igateProcess(incomingPacket);
                packet2Raw(tnc2, incomingPacket);
                tnc2.trim();
                log_i("[PKT] {\"dir\":\"rf_rx\",\"pkt\":\"%s\"}", tnc2.c_str());

                // store to log
                char call[11];
                if (incomingPacket.src.ssid > 0) {
                    sprintf(call, "%s-%d", incomingPacket.src.call, incomingPacket.src.ssid);
                } else {
                    sprintf(call, "%s", incomingPacket.src.call);
                }

                log_d("Call: %s", call);
                
                uint8_t type = pkgType((char *)incomingPacket.info);
                
                // char *rawP = (char *)malloc(tnc2.length());
                // memcpy(rawP, tnc2.c_str(), tnc2.length());
                // pkgListUpdate(call, rawP, type, 0);
                // free(rawP);
                pkgListUpdate(call, (char *)tnc2.c_str(), type, 0);
                strlcpy(lastRxCall, call, sizeof(lastRxCall));
                lastRxTime  = time(nullptr);
                lastRxIsIS  = false;

                bool isSmsForUs = (type & FILTER_MESSAGE) && config.aprs_sms_rx
                                  && aprsCheckMsg(call, (const char *)incomingPacket.info);
                bool msgForUs = isSmsForUs && config.aprs_sms_popup > 0;

                if (!msgForUs && config.aprs_rx_popup > 0) {
                    String msgType = "Type: " + pkgGetType(type);
                    OledPushMsg("RF RX", call, (char *)msgType.c_str(), config.aprs_rx_popup);
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
                        log_i("RF->INET: %s", tnc2.c_str());
#endif
                    }
                }

                // Digi Repeater Process
                if (callsignValid && config.tnc_digi) {
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
                                    digiDelay = random(10, 5000);
                                else if (digiCount > 10)
                                    digiDelay = random(10, 3000);
                                else if (digiCount > 0)
                                    digiDelay = random(10, 1500);
                                else
                                    digiDelay = random(10, 500);
                            } else {
                                digiDelay = random(10, config.digi_delay > 10 ? config.digi_delay : 20);
                            }
                        }
                        String digiPkg;
                        packet2Raw(digiPkg, incomingPacket);
                        pkgTxPush(digiPkg.c_str(), digiPkg.length(), digiDelay);
                    }
                    else
                    {
                        igateTLM.DROP++;
                        status.dropCount++;
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
// WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;

//bool wifiConnected = false;

void Wifi_connected(WiFiEvent_t event, WiFiEventInfo_t info){
  log_d("Successfully connected to Access Point");
}

void Get_IPAddress(WiFiEvent_t event, WiFiEventInfo_t info){
  log_d("WIFI is connected!");
  log_d("IP address: %s",WiFi.localIP().toString().c_str());
}

void Wifi_disconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  log_d("Disconnected from WIFI access point");
//   log_d("WiFi lost connection. Reason: ");
//   log_d("%s\n",info.wifi_sta_disconnected.reason);
  log_d("Reconnecting...");
}

void taskNetwork(void *pvParameters) {
    log_i("Task <Network> started");

    WiFi.onEvent(Wifi_connected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(Get_IPAddress, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(Wifi_disconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // WiFi.onEvent(Wifi_connected,SYSTEM_EVENT_STA_CONNECTED);
    // WiFi.onEvent(Get_IPAddress, SYSTEM_EVENT_STA_GOT_IP);
    // WiFi.onEvent(Wifi_disconnected, SYSTEM_EVENT_STA_DISCONNECTED);

    if (config.wifi_mode == WIFI_STA_FIX)
    { /**< WiFi station mode */
        WiFi.mode(WIFI_MODE_STA);
    }
    else if (config.wifi_mode == WIFI_AP_FIX)
    { /**< WiFi soft-AP mode */
        WiFi.mode(WIFI_MODE_AP);
    }
    else if (config.wifi_mode == WIFI_AP_STA_FIX)
    { /**< WiFi station + soft-AP mode */
        WiFi.mode(WIFI_MODE_APSTA);
    }
    else
    {
        WiFi.mode(WIFI_MODE_NULL);
    }

    if (config.wifi_mode & WIFI_STA_FIX)
    {
        // for (int i = 0; i < 5; i++)
        // {
        //     if (config.wifi_sta[i].enable)
        //     {
        //         wifiMulti.addAP(config.wifi_sta[i].wifi_ssid, config.wifi_sta[i].wifi_pass);
        //     }
        // }
        WiFi.setTxPower((wifi_power_t)config.wifi_power);
        // WiFi.setHostname("APRS_ESP_" + config.aprs_mycall);
        WiFi.setHostname("APRS_ESP");
    }

    if (config.wifi_mode & WIFI_AP_FIX)
    {
        WiFi.softAP(config.wifi_ap_ssid, config.wifi_ap_pass); // Start HOTspot removing password will disable security
        WiFi.softAPConfig(local_IP, gateway, subnet);
        log_i("Access point running. IP address: %s", WiFi.softAPIP().toString().c_str());
        webService();
    }

    // if (wifiMulti.run() == WL_CONNECTED)
    // {
    //     log_d("Wi-Fi CONNECTED!");
    //     log_d("IP address: %s", WiFi.localIP().toString().c_str());
    //     webService();
    //     NTP_Timeout = millis() + 2000;
    // }

    if (config.wifi_mode & WIFI_STA_FIX) {
        WiFi.begin(config.wifi_ssid, config.wifi_pass);
        WiFi.setAutoReconnect(false);  // after begin() — begin() resets this to true internally
        wifiTTL = millis() + connectTimeoutMs;
        if (!(config.wifi_mode & WIFI_AP_FIX)) {  // skip if webService() already called in AP block
            webService();
        }
        NTP_Timeout = millis() + 2000;
    }

    configTime(3600 * config.timeZone, 0, config.ntpServer);

    for (;;) {
        vTaskDelay(10 / portTICK_PERIOD_MS);

        if (config.wifi_mode != WIFI_OFF_FIX) {
            serviceHandle();
        }

        if (WiFi.status() == WL_CONNECTED) 
        {
            if (millis() > NTP_Timeout) {                    
                NTP_Timeout = millis() + 86400000;
                if (config.synctime) {
                    // Serial.println("Config NTP");
                    // setSyncProvider(getNtpTime);
                    log_i("Contacting NTP server");
                    configTime(3600 * config.timeZone, 0, config.ntpServer);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    time_t systemTime;
                    time(&systemTime);
                    setTime(systemTime);
                    if (systemUptime == 0) {
                        systemUptime = time(NULL);
                    }

                    timeSyncFlag = T_SYNC_NTP;
                }
            }

            if (config.aprs) {
                if (aprsClient.connected() == false) {
                    APRSConnect();
                } else {
                    if (aprsClient.available()) {
                        String line = aprsClient.readStringUntil('\n');  //read the value at Server answer sleep line by line
                        log_i("APRS-IS: %s", line.c_str());
                        status.isCount++;

                        int start_val = line.indexOf(">", 0);  // find the first position of >
                        if (start_val > 3) {
                            // raw = (char *)malloc(line.length() + 1);
                            String src_call = line.substring(0, start_val);
                            int msg_call_idx = line.indexOf("::");  // text only
                            String msg_call = "";
                            if (msg_call_idx > 0) {
                                msg_call = line.substring(msg_call_idx + 2, msg_call_idx + 9);
                            }

                            log_i("SRC_CALL: %s", src_call.c_str());
                            if (msg_call_idx > 0) {
                                log_i("MSG_CALL: %s", msg_call.c_str());
                            }
                            
                            char raw[500];
                            memset(&raw[0], 0, sizeof(raw));
                            start_val = line.indexOf(":", 10); // Search of info in ax25
                            if (start_val > 5)
                            {
                                String info = line.substring(start_val + 1);
                                // info.toCharArray(&raw[0], info.length(), 0);
                                memcpy(raw, info.c_str(), info.length());
                                uint16_t type = pkgType(&raw[0]);
                                log_d("Type: %d", type);
                                pkgListUpdate((char *)src_call.c_str(), (char *)line.c_str(), type, 1);
                                strlcpy(lastRxCall, src_call.c_str(), sizeof(lastRxCall));
                                lastRxTime = time(nullptr);
                                lastRxIsIS = true;

                                bool isSmsForUs2 = (type & FILTER_MESSAGE) && config.aprs_sms_rx
                                                   && aprsCheckMsg(src_call.c_str(), &raw[0]);
                                bool msgForUs2 = isSmsForUs2 && config.aprs_sms_popup > 0;

                                if (!msgForUs2 && config.aprs_rx_popup > 0) {
                                    String msgType = "Type: " + pkgGetType(type);
                                    OledPushMsg("APRS-IS RX", (char *)src_call.c_str(), (char *)msgType.c_str(), config.aprs_rx_popup);
                                }
                            }

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
                                if (callsignValid && config.tnc && config.inet2rf && (msg_call != "")) {
                                    // Is adresse is in owner callsigns group?
                                    if (msg_call.startsWith(config.aprs_mycall)) {
                                        log_i("MSG to Owner group");
                                        pkgTxPush(line.c_str(), line.length(), 0);
                                        status.inet2rf++;
                                        igateTLM.INET2RF++;
                                        log_i("[PKT] {\"dir\":\"inet2rf\",\"pkt\":\"%s\"}", line.c_str());
                                    } else {
                                        bool msgForwarded = false;
                                        // Is it a message for last heard stations?
                                        for (int i = 0; i < PKGLISTSIZE; i++) {
                                            if (pkgList[i].time > 0) {
                                                if (strcmp(pkgList[i].calsign, msg_call.c_str()) == 0) {
                                                    if (pkgList[i].channel == 0) {  // was heard on RF
                                                        log_i("[PKT] {\"dir\":\"inet2rf\",\"reason\":\"last_heard\"}");
                                                        pkgTxPush(line.c_str(), line.length(), 0);
                                                        status.inet2rf++;
                                                        igateTLM.INET2RF++;
                                                        log_i("[PKT] {\"dir\":\"inet2rf\",\"pkt\":\"%s\"}", line.c_str());
                                                        msgForwarded = true;
                                                        break;
                                                    }
                                                }
                                            }
                                        }

                                        if (!msgForwarded) {
                                            // Not found in last heard list
                                            status.dropCount++;
                                            log_i("[PKT] {\"dir\":\"drop\",\"reason\":\"not_last_heard\"}");
                                        }
                                    }
                                } else {
                                    // No INET2RF configured or MSG_CALL not present
                                    status.dropCount++;
                                    log_i("[PKT] {\"dir\":\"drop\",\"reason\":\"no_inet2rf\",\"from\":\"%s\"}", src_call.c_str());
                                }
                            } else {
                                // Telemetry found
                                igateTLM.DROP++;
                                status.dropCount++;
                                log_i("[PKT] {\"dir\":\"drop\",\"reason\":\"telemetry\",\"from\":\"%s\"}", src_call.c_str());
                            }
                        }
                    }
                }
            }
        }

        if ((config.wifi_mode & WIFI_STA_FIX) && WiFi.status() != WL_CONNECTED) {
            if (millis() > wifiTTL) {
                wifiTTL = millis() + 30000;
                WiFi.reconnect();
                log_d("WiFi STA reconnect attempt");
            }
        }
    }
}

void taskOLEDDisplay(void *pvParameters) {
    log_i("Task <OLEDDisplay> started");
    unsigned long lastFullUpdate = 0;

    for (;;) {
        vTaskDelay(50 / portTICK_PERIOD_MS);

        if (fwUpdateProcess) {
            dbgTick();
            OledUpdateFWU();
            continue;
        }

        bool fullCycle   = (millis() - lastFullUpdate >= 1000);
        bool doOled      = menuUpdateNeeded || fullCycle;
        static uint32_t lastStatusLog  = 0;
        static bool     cfgBackupDone  = false;

        if (fullCycle) {
            lastFullUpdate = millis();
            dbgTick();
            if (millis() - lastStatusLog >= 10000) {
                lastStatusLog = millis();
                SerialStatusLog();
            }
            if (!cfgBackupDone && millis() >= 5UL * 60UL * 1000UL) {
                SaveConfigBackup();
                cfgBackupDone = true;
            }
        }


        if (doOled && !oledTxBusy) {
            menuUpdateNeeded = false;
#if defined(ADC_BATTERY)
            OledUpdate(batteryPercentage, false, AFSKInitAct);
            if (fullCycle) WebDataUpdate(batteryVoltage, false);
#elif defined(USE_PMU)
            OledUpdate(batteryPercentage, vbusIn, AFSKInitAct, PMU.isCharging());
            if (fullCycle) WebDataUpdate(batteryPercentage, vbusIn);
#else
            OledUpdate(-1, false, AFSKInitAct);
            if (fullCycle) WebDataUpdate(-1, false);
#endif
        }

        if (fullCycle && (menuMode == MENU_RX_LIST || menuMode == MENU_RX_DETAIL)) {
            sort(pkgList, PKGLISTSIZE);
            rxListCount = pkgListValidCount();
        }

        if (fullCycle) {
            if (!oledTxBusy) OledCheckAutoDim();
#if defined(USE_PMU)
            if (!oledTxBusy) loopPMU();
#endif
#if defined(USE_NEOPIXEL)
            strip.show();
#endif
        }
    }
}

void updateGps(void) {
    if (fwUpdateProcess) return;

    // If startup dealy not expired
    if (!gpsUnlock) {
        if (millis() < gpsUpdTMO) {
            return;
        } else {
            log_i("GPS Unlocked");
            gpsUnlock = true;
        }
    }

    GpsUpdate();

    // 1/sec
    if (millis() - gpsUpdTMO > 1000) {
        distanceChanged();
        gpsUpdTMO = millis();
    }
}

void taskGPS(void *pvParameters) {
    log_i("Task <GPS> started");

    for (;;) {
        vTaskDelay(100 / portTICK_PERIOD_MS);

#ifdef USE_GPS
        // if (SerialGPS.available()) {
        //     String gpsData = SerialGPS.readStringUntil('\n');
        //     log_d("%s", gpsData.c_str());
        // }
#endif

        updateGps();
    }
}

#if !defined(BOARD_ESP32DR)
void taskTNC(void *pvParameters) {
    log_i("Task <TNC> started");

    for (;;) {
        if (!fwUpdateProcess) {
            if (AFSKInitAct == true) {
               AFSK_Poll(true);
            }
        }
#if defined(BOARD_ESP32DR)
        vTaskDelay(5 / portTICK_PERIOD_MS);
#else
        vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
    }
}
#endif
