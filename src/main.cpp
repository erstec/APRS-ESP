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

#ifdef USE_SCREEN
#include "Wire.h"
#include "Adafruit_GFX.h"
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSerifItalic9pt7b.h>
// #include <Fonts/Seven_Segment24pt7b.h>
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

#if defined(TX_LED_PIN)

#if defined(INVERT_LEDS)
#if defined(USE_NEOPIXEL)
#define TX_LED_ON() digitalWrite(TX_LED_PIN, LOW); strip.setPixelColor(0, 255, 0, 0); strip.show(); // Red
#define TX_LED_OFF() digitalWrite(TX_LED_PIN, HIGH); strip.setPixelColor(0, 0, 0, 0); strip.show(); // Off
#else
#define TX_LED_ON() digitalWrite(TX_LED_PIN, LOW);
#define TX_LED_OFF() digitalWrite(TX_LED_PIN, HIGH);
#endif
#else
#if defined(USE_NEOPIXEL)
#define TX_LED_ON() digitalWrite(TX_LED_PIN, HIGH); strip.setPixelColor(0, 255, 0, 0); strip.show();    // Red
#define TX_LED_OFF() digitalWrite(TX_LED_PIN, LOW); strip.setPixelColor(0, 0, 0, 0); strip.show();      // Off
#else
#define TX_LED_ON() digitalWrite(TX_LED_PIN, HIGH);
#define TX_LED_OFF() digitalWrite(TX_LED_PIN, LOW);
#endif
#endif

#else

#if defined(USE_NEOPIXEL)
#define TX_LED_ON() strip.setPixelColor(0, 255, 0, 0); strip.show();    // Red
#define TX_LED_OFF() strip.setPixelColor(0, 0, 0, 0); strip.show();     // Off
#endif

#endif

#ifdef USE_RF
HardwareSerial SerialRF(SERIAL_RF_UART);
#endif

bool fwUpdateProcess = false;

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
TaskHandle_t taskOLEDDisplayHandle;

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

teTimeSync timeSyncFlag = T_SYNC_NONE;

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
                if (config.rf_power) {
#if defined(BOARD_TTWR_PLUS_MOD)
                    pinMode(POWER_PIN, OUTPUT);
                    digitalWrite(POWER_PIN, HIGH);
#else
                    pinMode(POWER_PIN, OPEN_DRAIN);
#endif
                } else {
                    pinMode(POWER_PIN, OUTPUT);
                    digitalWrite(POWER_PIN, LOW);  // RF Power
                }

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

    Serial.printf("APRS Time: %02d %02d:%02d\r\n", day, hour, min);

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
#ifdef USE_KISS
    kiss_messageCallback(ctx);
#endif
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
//TODO processPacket();
    
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
        if (strlen(config.aprs_object) >= 3) {
            uint16_t passcode = aprsParse.passCode(config.aprs_object);
            login = "user " + String(config.aprs_object) + " pass " + String(passcode, DEC) + " vers ESP32IGate V" + String(VERSION) + " filter " + String(config.aprs_filter);
        } else {
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
        }
        aprsClient.println(login);
        // Serial.println(login);
        // Serial.println("Success");
        delay(500);
    }

    return true;
}

#if defined(USE_PMU)
bool vbusIn = false;
bool pmu_flag = false;

void setFlag(void)
{
    pmu_flag = true;
}

void setupPower()
{
    bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
    if (result == false) {
        while (1) {
            Serial.println("PMU is not online...");
            delay(500);
        }
    }

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_3V88);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);


    // Get the VSYS shutdown voltage
    uint16_t vol = PMU.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);


    //! DC1 ESP32S3 Core VDD , Don't change
    // PMU.setDC1Voltage(3300);

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



    Serial.println("DCDC=======================================================================");
    Serial.printf("DC1  : %s   Voltage:%u mV \n",  PMU.isEnableDC1()  ? "+" : "-", PMU.getDC1Voltage());
    Serial.printf("DC2  : %s   Voltage:%u mV \n",  PMU.isEnableDC2()  ? "+" : "-", PMU.getDC2Voltage());
    Serial.printf("DC3  : %s   Voltage:%u mV \n",  PMU.isEnableDC3()  ? "+" : "-", PMU.getDC3Voltage());
    Serial.printf("DC4  : %s   Voltage:%u mV \n",  PMU.isEnableDC4()  ? "+" : "-", PMU.getDC4Voltage());
    Serial.printf("DC5  : %s   Voltage:%u mV \n",  PMU.isEnableDC5()  ? "+" : "-", PMU.getDC5Voltage());
    Serial.println("ALDO=======================================================================");
    Serial.printf("ALDO1: %s   Voltage:%u mV\n",  PMU.isEnableALDO1()  ? "+" : "-", PMU.getALDO1Voltage());
    Serial.printf("ALDO2: %s   Voltage:%u mV\n",  PMU.isEnableALDO2()  ? "+" : "-", PMU.getALDO2Voltage());
    Serial.printf("ALDO3: %s   Voltage:%u mV\n",  PMU.isEnableALDO3()  ? "+" : "-", PMU.getALDO3Voltage());
    Serial.printf("ALDO4: %s   Voltage:%u mV\n",  PMU.isEnableALDO4()  ? "+" : "-", PMU.getALDO4Voltage());
    Serial.println("BLDO=======================================================================");
    Serial.printf("BLDO1: %s   Voltage:%u mV\n",  PMU.isEnableBLDO1()  ? "+" : "-", PMU.getBLDO1Voltage());
    Serial.printf("BLDO2: %s   Voltage:%u mV\n",  PMU.isEnableBLDO2()  ? "+" : "-", PMU.getBLDO2Voltage());
    Serial.println("===========================================================================");

    // Set the time of pressing the button to turn off
    PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU.getPowerKeyPressOffTime();
    Serial.print("PowerKeyPressOffTime:");
    switch (opt) {
    case XPOWERS_POWEROFF_4S: Serial.println("4 Second");
        break;
    case XPOWERS_POWEROFF_6S: Serial.println("6 Second");
        break;
    case XPOWERS_POWEROFF_8S: Serial.println("8 Second");
        break;
    case XPOWERS_POWEROFF_10S: Serial.println("10 Second");
        break;
    default:
        break;
    }
    // Set the button power-on press time
    PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
    opt = PMU.getPowerKeyPressOnTime();
    Serial.print("PowerKeyPressOnTime:");
    switch (opt) {
    case XPOWERS_POWERON_128MS: Serial.println("128 Ms");
        break;
    case XPOWERS_POWERON_512MS: Serial.println("512 Ms");
        break;
    case XPOWERS_POWERON_1S: Serial.println("1 Second");
        break;
    case XPOWERS_POWERON_2S: Serial.println("2 Second");
        break;
    default:
        break;
    }

    Serial.println("===========================================================================");
    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU.disableTSPinMeasure();


    // Enable internal ADC detection
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();


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
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ       //CHARGE
    );

    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_150MA);

    // Set constant current charge current limit
    //! Using inferior USB cables and adapters will not reach the maximum charging current.
    //! Please pay attention to add a suitable heat sink above the PMU when setting the charging current to 1A
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA);

    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_150MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    // Disable the PMU long press shutdown function
    PMU.disableLongPressShutdown();


    // Get charging target current
    const uint16_t currTable[] = {
        0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000
    };
    uint8_t val = PMU.getChargerConstantCurr();
    Serial.print("Val = "); Serial.println(val);
    Serial.print("Setting Charge Target Current : ");
    Serial.println(currTable[val]);

    // Get charging target voltage
    const uint16_t tableVoltage[] = {
        0, 4000, 4100, 4200, 4350, 4400, 255
    };
    val = PMU.getChargeTargetVoltage();
    Serial.print("Setting Charge Target Voltage : ");
    Serial.println(tableVoltage[val]);

}
#endif

// #include "soc/soc.h"
// #include "soc/rtc_cntl_reg.h"

void setup()
{
    // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

    pinMode(BOOT_PIN, INPUT_PULLUP);  // BOOT Button

    // Set up serial port
    Serial.setRxBufferSize(256);
    Serial.begin(SERIAL_DEBUG_BAUD);  // debug

#if defined(USE_TNC)
    SerialTNC.setRxBufferSize(500);
    SerialTNC.begin(SERIAL_TNC_BAUD, SERIAL_8N1, SERIAL_TNC_RXPIN,
                    SERIAL_TNC_TXPIN);
#endif

#if defined(USE_GPS)
    SerialGPS.setRxBufferSize(500);
    SerialGPS.begin(SERIAL_GPS_BAUD, SERIAL_8N1, SERIAL_GPS_RXPIN,
                    SERIAL_GPS_TXPIN);
#endif

#ifndef USE_ROTARY
    pinMode(PIN_ROT_BTN, INPUT_PULLUP);
#endif

    Serial.println();
    Serial.println("Start APRS-ESP V" + String(VERSION_FULL));
    Serial.println("Press and hold BOOT button for 3 sec to Factory Default config");

#if defined(BOARD_TTWR_MOD)
    // +4.2V EN
    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);

    // OLED EN
    pinMode(23, OUTPUT);
    digitalWrite(23, HIGH);
#endif

#if defined(ADC_BATTERY)
    // Battery Voltage
    pinMode(ADC_BATTERY, INPUT);
#endif

#ifdef USE_SCREEN
    OledStartup();
#endif

#if defined(USE_PMU)
    // PMU
    setupPower();
#endif

#if defined(BOARD_TTWR_PLUS_MOD)
    // MIC Select
    pinMode(MIC_CH_SEL, OUTPUT);
    digitalWrite(MIC_CH_SEL, HIGH);  // LOW - MIC / HIGH - ESP32

    // NeoPixel
    strip.setBrightness(100);
    strip.begin();

    strip.setPixelColor(0, 0, 0, 255);  // Blue
    strip.show();
#endif

    LoadConfig();

#ifdef USE_RF
    // RF SHOULD BE Initialized or there is no reason to startup at all
    while (!RF_Init(true)) {};
#endif

#if defined(USE_PMU)
    if (PMU.isVbusIn()) {
        vbusIn = true;
        Serial.println("Vbus In");
        // PMU.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);
    }
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
                            (32768),       /* Stack size in words */
                            NULL,          /* Task input parameter */
                            1,             /* Priority of the task */
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
}

int pkgCount = 0;

String send_gps_location() {
    String tnc2Raw = "";
    
    float _lat = 0.0;
    float _lon = 0.0;

    if (config.gps_mode == GPS_MODE_AUTO) {
        Serial.println("GPS Mode: Auto");
        //if (/*age != (uint32_t)ULONG_MAX &&*/ gps.location.isValid() /*|| gotGpsFix*/) {
        if (gps.location.isValid()) {
            _lat = lat / 1000000.0;
            _lon = lon / 1000000.0;
            distance = 0;   // Reset counted distance
            Serial.println("GPS Fix, using current location");
        } else {
            _lat = config.gps_lat;
            _lon = config.gps_lon;
            Serial.println("No GPS Fix, using fixed location");
        }
    } else if (config.gps_mode == GPS_MODE_FIXED) {
        Serial.println("GPS Mode: Fixed Only");
        _lat = config.gps_lat;
        _lon = config.gps_lon;
    } else if (config.gps_mode == GPS_MODE_GPS) {
        Serial.println("GPS Mode: GPS Only");
        if (gps.location.isValid()) {
            _lat = lat / 1000000.0;
            _lon = lon / 1000000.0;
            distance = 0;   // Reset counted distance
            Serial.println("GPS Fix, using current location");
        } else {
            Serial.println("No GPS Fix, skipped");
            return "";
        }
    }

    // Reset counted distance
    distance = 0;

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
        sprintf(strtmp, "%s>APESP1", config.aprs_mycall);
    else
        sprintf(strtmp, "%s-%d>APESP1", config.aprs_mycall, config.aprs_ssid);

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

void printPeriodicDebug() {
#if defined(ADC_BATTERY)
    if (config.wifi_mode == WIFI_OFF) {
        // batteryVoltage = (analogRead(ADC_BATTERY) * 2);
        // batteryVoltage += (batteryVoltage > 0 ? BATT_OFFSET : 0);

        batteryPercentage = getBatteryPercentage();
    } else {
        batteryPercentage = digitalRead(ADC_BATTERY);
    }

#endif
#if defined(USE_PMU)
    batteryPercentage = getBatteryPercentage();
#endif
    printTime();
#if defined(ADC_BATTERY) || defined(USE_PMU)
    Serial.print("Bat: ");
#endif
#if defined(ADC_BATTERY)
    Serial.print(batteryVoltage);
    Serial.print("mV ");
#endif
#if defined(ADC_BATTERY) || defined(USE_PMU)
    Serial.print(batteryPercentage);
    Serial.print("%");
#endif
#if defined(ADC_BATTERY) || defined(USE_PMU)
    Serial.print(", lat: ");
#else
    Serial.print("Lat: ");
#endif
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
        scrUpdTMO = millis();

        printPeriodicDebug();
    }

    if (fwUpdateProcess) return;

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

#if defined(USE_PMU)
void loopPMU()
{
    if (!pmu_flag) {
        return;
    }

    pmu_flag = false;
    // Get PMU Interrupt Status Register
    uint32_t status = PMU.getIrqStatus();
    Serial.print("STATUS => HEX:");
    Serial.print(status, HEX);
    Serial.print(" BIN:");
    Serial.println(status, BIN);

    // if (PMU.isPekeyShortPressIrq()) {
    //     volumeUp();
    // }

    if (PMU.isBatChagerStartIrq() || PMU.isBatteryConnect()) {
        Serial.println("Battery Charging");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);
    }

    if (PMU.isBatChagerDoneIrq()) {
        Serial.println("Battery Charging Done");
    }

    if (PMU.isBatRemoveIrq()) {
        Serial.println("Battery Removed");
        PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
    }

    if (PMU.isVbusInsertIrq()) {
        Serial.println("USB Detected");
        vbusIn = true;
    }

    if (PMU.isVbusRemoveIrq()) {
        Serial.println("USB Removed");
        vbusIn = false;
    }

    if (PMU.isPekeyLongPressIrq()) {
    //     vTaskDelete(dataTaskHandler);
    //     vTaskDelete(clearvolumeSliderTaskHandler);
    //     vTaskDelete(gnssTaskHandler);
    //     vTaskDelete(encoderTaskHandler);
    //     vTaskDelete(updateTaskHandler);
    //     vTaskDelete(pielxTaskHandler);

    //     u8g2.setFont(u8g2_font_timR14_tf);
    //     u8g2.clearBuffer();
    //     int8_t height = 30 + u8g2.getAscent();
    //     u8g2.drawStr(20, height, "Power OFF");
    //     u8g2.sendBuffer();
    //     delay(3000);
        PMU.shutdown();
    }

    // Clear PMU Interrupt Status Register
    PMU.clearIrqStatus();
}
#endif

long sendTimer = 0;
bool AFSKInitAct = false;
int btn_count = 0;
long timeCheck = 0;
long TimeSyncPeriod = 0;

void loop()
{
    vTaskDelay(5 / portTICK_PERIOD_MS);  // 5 ms // remove?
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
                Serial.println("TimeSync Flag Reset");
            }
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
            AFSK_Poll(true);
#else
            AFSK_Poll(false, LOW);
#endif
#ifdef USE_GPS
            // if (SerialGPS.available()) {
            //     String gpsData = SerialGPS.readStringUntil('\n');
            //     Serial.print(gpsData);
            // }
#endif
        }

    } // if (!fwUpdateProcess)
        
    bool update_screen = RotaryProcess();

    // if (send_aprs_update) {
    //     update_screen |= true;
    //     // send_aprs_update = false;    // moved to APRS task
    // }
    // if (update_screen) OledUpdate();
    
    updateScreenAndGps(update_screen);

    if (fwUpdateProcess) return;

    uint8_t bootPin2 = HIGH;
#ifndef USE_ROTARY
    bootPin2 = digitalRead(PIN_ROT_BTN);
#endif

    if (digitalRead(BOOT_PIN) == LOW || bootPin2 == LOW) {
        btn_count++;
        if (btn_count > 1000 && btn_count < 2000)  // Push BOOT 10sec
        {
            RX_LED_ON();
            TX_LED_ON();
        }
        if (btn_count > 2000)  // Push BOOT 20sec
        {
            RX_LED_OFF();
            TX_LED_OFF();
        }
    } else {
        if (btn_count > 0) {
            // Serial.printf("btn_count=%dms\n", btn_count * 10);
            if (btn_count > 1000)  // Push BOOT 10sec to Disable/Enable WiFi
            {
                if (config.wifi_mode == WIFI_OFF) {
                    config.wifi_mode = WIFI_AP_STA_FIX;
                    Serial.println("WiFi ON");
                } else {
                    config.wifi_mode = WIFI_OFF;
                    Serial.println("WiFi OFF");
                }
                btn_count = 0;
                SaveConfig();
                esp_restart();
            }
            else if (btn_count > 2000) {    // Config Default
                Serial.println("Factory Default");
                btn_count = 0;
                DefaultConfig();
                SaveConfig();
                esp_restart();
            }
            else if (btn_count > 10)  // Push BOOT >100mS to PTT Fix location
            {
                if (config.tnc) {
                    String tnc2Raw = send_gps_location();
                    if (tnc2Raw.length() > 0) {
                        pkgTxUpdate(tnc2Raw.c_str(), 0);
                        // APRS_sendTNC2Pkt(tnc2Raw); // Send packet to RF
#ifdef DEBUG_TNC
                        Serial.println("Manual TX: " + tnc2Raw);
                    }
#endif
                }
            }
            btn_count = 0;
        }
    }

#if defined(USE_PMU)
    loopPMU();
#endif
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

    sprintf(str, "%s-%d>APESP1%s::%s:ack%d", config.aprs_mycall, config.aprs_ssid, VERSION, call, msgId);
    //	client.println(str);
    return String(str);
}

void sendIsPkg(char *raw) {
    char str[300];
    sprintf(str, "%s-%d>APESP1%s:%s", config.aprs_mycall, config.aprs_ssid, VERSION, raw);
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
        sprintf(str, "%s>APESP1::%s:%s", config.aprs_mycall, call, raw);
    else
        sprintf(str, "%s-%d>APESP1::%s:%s", config.aprs_mycall, config.aprs_ssid, call, raw);

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
                    if (tnc2Raw.length() > 0) {
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
            }
            // send_gps_location();
            //  APRS_setCallsign(config.aprs_mycall, config.aprs_ssid);
            //  	APRS_setPath1("WIDE1", 1);
            //  	APRS_setPreamble(APRS_PREAMBLE);
            //  	APRS_setTail(APRS_TAIL);
            // APRS_sendTNC2Pkt("HS5TQA-6>APESP1,TRACE2-2:=1343.76N/10026.06E&ESP32
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
                    sprintf(rawTlm, "%s>APESP1:T#%03d,%d,%d,%d,%d,%d,00000000",
                            config.aprs_mycall, igateTLM.Sequence,
                            igateTLM.RF2INET, igateTLM.INET2RF, igateTLM.RX,
                            igateTLM.TX, igateTLM.DROP);
                } else {
                    sprintf(
                        rawTlm, "%s-%d>APESP1:T#%03d,%d,%d,%d,%d,%d,00000000",
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

                // store to log
                char call[11];
                if (incomingPacket.src.ssid > 0) {
                    sprintf(call, "%s-%d", incomingPacket.src.call, incomingPacket.src.ssid);
                } else {
                    sprintf(call, "%s", incomingPacket.src.call);
                }
                
                uint8_t type = pkgType((char *)incomingPacket.info);
                pkgListUpdate(call, type);

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

static bool wiFiActive = false;
bool wifiConnected = false;

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
        wiFiActive = true;
    } else if (config.wifi_mode == WIFI_STA_FIX) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        Serial.println(F("WiFi Station Only mode"));
        wiFiActive = true;
    } else {
        WiFi.mode(WIFI_OFF);
        WiFi.disconnect(true);
        delay(100);
        // Serial.println(F("WiFi OFF. BT only mode"));
        Serial.println(F("WiFi OFF All mode"));
        // SerialBT.begin("ESP32TNC");
        wiFiActive = false;
    }

    if (wiFiActive) {
        webService();
    }

    configTime(3600 * config.timeZone, 0, config.ntpServer);

    for (;;) {
        // wdtNetworkTimer = millis();
        vTaskDelay(5 / portTICK_PERIOD_MS);

        if (wiFiActive) {
            serviceHandle();
        }

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

                    if (!wifiConnected) {
                        if (WiFi.status() == WL_CONNECTED) {
                            wifiConnected = true;
                        }
                    }

                    Serial.println("WiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());

                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    NTP_Timeout = millis() + 5000;
#ifndef I2S_INTERNAL
                    AFSK_TimerEnable(true);
#endif
                }
            } else {
                if (millis() > NTP_Timeout) {                    
                    NTP_Timeout = millis() + 86400000;
                    if (config.synctime) {
                        // Serial.println("Config NTP");
                        // setSyncProvider(getNtpTime);
                        Serial.println("Setting up NTP");
                        configTime(3600 * config.timeZone, 0, config.ntpServer);
                        vTaskDelay(3000 / portTICK_PERIOD_MS);
                        time_t systemTime;
                        time(&systemTime);
                        setTime(systemTime);
                        if (systemUptime == 0) {
                            systemUptime = now();
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
#ifdef DEBUG_IS
                            printTime();
                            Serial.print("APRS-IS: ");
                            Serial.println(line);
#endif
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
                                    status.dropCount++;
                                    Serial.print("INET Message TELEMETRY from ");
                                    Serial.println(src_call);
                                }
                            }
                        }
                    }
                }

                if (WiFi.status() != WL_CONNECTED && wifiConnected) {
                    Serial.println("WiFi disconnected");
                    wifiConnected = false;
                    WiFi.disconnect();
                    wifiTTL = 0;
                }
            }
        }
    }
}

void taskOLEDDisplay(void *pvParameters) {

    Serial.println("Task <OLEDDisplay> started");

    for (;;) {
        if (fwUpdateProcess) {
            OledUpdateFWU();
            continue;
        }

#if defined(ADC_BATTERY)
        OledUpdate(batteryPercentage, false);
#elif defined(USE_PMU)
        OledUpdate(batteryPercentage, vbusIn);
#else
        OledUpdate(-1, false);
#endif

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
