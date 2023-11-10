/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with OLED Display.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "oled.h"
#include "AFSK.h"
#include "TinyGPS++.h"
#include "WiFi.h"
#include "config.h"
#include "gps.h"
#include "main.h"

#include <Fonts/FreeSans9pt7b.h>

#include <Wire.h>
#if defined(USE_SCREEN_SSD1306)
#include "Adafruit_SSD1306.h"
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST_PIN);
#elif defined(USE_SCREEN_SH1106)
#include <Adafruit_SH1106.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
Adafruit_SH1106 display(OLED_SDA_PIN, OLED_SCL_PIN);
#endif

extern TinyGPSPlus gps;
extern Configuration config;
extern WiFiClient aprsClient;

static struct {
    char caption[16];
    char msg[16];
    char msg2[16];
    uint8_t timeout;
    // time_t time;
} msgBox;

void OledStartup() {
#ifdef USE_SCREEN
#if defined(USE_SCREEN_SSD1306)
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false)) {
        log_e("SSD1306 init failed");
    } else {
        log_i("SSD1306 init ok");
    }
#elif defined(USE_SCREEN_SH1106)
    log_i("SH1106 init");
    display.begin(SH1106_SWITCHCAPVCC, 0x3C);
    display.stopscroll();
#endif

    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);
    display.setTextSize(1);
    display.setTextWrap(false);
    display.cp437(true);

    char buf[16];
    sprintf(buf, "APRS-ESP V%s", VERSION);
    display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, 0);
    display.print(buf);

    sprintf(buf, "Boot...");
    display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, CHAR_HEIGHT * 2);
    display.print(buf);

    display.display();
#endif
}

void OledPostStartup(String customMsg) {
    char buf[16];
    sprintf(buf, customMsg.c_str());
    display.fillRect(0, CHAR_HEIGHT * 4, display.width(), CHAR_HEIGHT * 5, BLACK);
    display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, CHAR_HEIGHT * 4);
    display.print(buf);

    display.display();
}

// draw rectangle box with message
static void OledDrawMsg() {
    if (msgBox.timeout > 0) {
        display.fillRect(10 - 2, 5 - 2, display.width() - 20 + 2, display.height() - 10 + 2, BLACK);
        display.drawRect(10, 5, display.width() - 20, display.height() - 10, WHITE);
        // caption
        display.setCursor(display.width() / 2 - strlen(msgBox.caption) * CHAR_WIDTH / 2, 10);
        display.print(msgBox.caption);
        // message
        display.setFont(&FreeSans9pt7b);
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(msgBox.msg, 0, 0, &x1, &y1, &w, &h);
        if (w > display.width() - 10 - 10) w = display.width();
        display.setCursor(display.width() / 2 - w / 2, 36);
        display.print(msgBox.msg);
        // second message line
        display.setFont();
        display.setCursor(display.width() / 2 - strlen(msgBox.msg2) * CHAR_WIDTH / 2, 45);
        display.print(msgBox.msg2);

        msgBox.timeout--;
    }
}

void OledUpdate(int batData, bool usbPlugged) {
#ifdef USE_SCREEN
    if (AFSK_modem->sending) return;
    // if (AFSK_modem->hdlc.receiving) return;

    char buf[24];

    bool isValid = gps.location.isValid();
    uint32_t satCnt = gps.satellites.value();

    display.clearDisplay();
    // display.setTextColor(WHITE, BLACK);
    // display.setTextSize(1);
    //display.setFont
    // display.invertDisplay(false);

    // WiFi IP printed from task, but because we are clearing screen draw it again
    display.setCursor(0, CHAR_HEIGHT * 1);
    if (config.wifi_mode == WIFI_STA_FIX) {
        display.print(WiFi.localIP());
    } else if (config.wifi_mode == WIFI_AP_STA_FIX || config.wifi_mode == WIFI_AP_FIX) {
        if (WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            display.print(WiFi.localIP());
        } else {
            display.print(WiFi.softAPIP());
        }
    } else {
        display.print("No IP - WiFi OFF");
    }

    // DateTime / Battery
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    if (tmstruct.tm_hour > 25 || tmstruct.tm_min > 60 || tmstruct.tm_sec > 60) {
        sprintf(buf, "NO TIME");
    } else {
        sprintf(buf, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    }
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2) , CHAR_HEIGHT * 2);   // center on the screen
    display.print(buf);
    // Timesync source
    display.setCursor((display.width() / 2) + (strlen(buf) * CHAR_WIDTH / 2) + (CHAR_WIDTH / 2) - 1, CHAR_HEIGHT * 2);
    if (timeSyncFlag == T_SYNC_NTP) {
        display.print("NTP");
    } else if (timeSyncFlag == T_SYNC_GPS) {
        display.print("GPS");
    } else if (timeSyncFlag == T_SYNC_APRS) {
        display.print("APRS");
    } else {
        display.print("NO");
    }

    display.setCursor(0, CHAR_HEIGHT * 2);
    display.print("B:");
#if defined(ADC_BATTERY)
    if (config.wifi_mode == WIFI_OFF) {
#endif
        if (batData >= 0) {
            display.print(batData);
            display.print("%");
        } else {
            display.print("NA");
        }
#if defined(ADC_BATTERY)
    } else {
        if (batData == 1) {
            display.print("YES");
        } else {
            display.print("NO");
        }
    }
#endif

    display.setCursor(display.width() - CHAR_WIDTH * 3, CHAR_HEIGHT * 2);
    if (usbPlugged) {
        display.print("USB");
    } else {
#if defined(USE_PMU)
        display.print("BAT");
#else
        display.print("   ");
#endif
    }

    // Main section
    // Top line
    display.setCursor(0, 0);
    display.printf("%s-%d>%s", config.aprs_mycall, config.aprs_ssid, config.aprs_path);

    display.setCursor(display.width() - CHAR_WIDTH * 1, 0);
    switch (config.gps_mode) {
        case GPS_MODE_AUTO:
            display.print("A");
            break;
        case GPS_MODE_GPS:
            display.print("G");
            break;
        case GPS_MODE_FIXED:
            display.print("F");
            break;
    }

    // Second line
    if (config.aprs) {
        display.setCursor(display.width() - CHAR_WIDTH * 5, CHAR_HEIGHT * 1);
        display.print(aprsClient.connected() ? "A+" : "A-");
    }

    if (config.wifi_mode != WIFI_OFF) {
        display.setCursor(display.width() - CHAR_WIDTH * 2, CHAR_HEIGHT * 1);
        display.print(WiFi.status() == WL_CONNECTED ? "W+" : "W-");
    }

    // Configuration Section
    // Tx interval or SB - SmartBeaconing
    display.setCursor(0, display.height() - CHAR_HEIGHT * 5);
    if (config.aprs_beacon > 0) {
        display.printf("T%d", config.aprs_beacon);
    } else {
        display.print("SB");
    }
    // TN - TNC, TM - Telemetry, DG - Digipeater, IS - APRS-IS, RI - RF->IGate, IR - IGate->RF
    sprintf(buf, "%s%s%s%s%s%s", 
            (config.tnc ? " TN" : ""),
            (config.tnc_telemetry ? " TM" : ""),
            (config.tnc_digi ? " DG" : ""),
            (config.aprs ? " IS" : ""),
            (config.rf2inet ? " RI" : ""),
            (config.inet2rf ? " IR" : ""));
    display.setCursor(display.width() - strlen(buf) * CHAR_WIDTH, display.height() - CHAR_HEIGHT * 5);
    display.print(buf);
    
    // GPS Section
    // 1st line
    // Sat count, fix status
    display.setCursor(0, display.height() - CHAR_HEIGHT * 4);
    display.printf("%d%s ", gps.satellites.value(), isValid ? "+" : "-");

    // altitude
    sprintf(buf, "%.1fm", gps.altitude.meters());
    display.setCursor(display.width() - CHAR_WIDTH * strlen(buf), display.height() - CHAR_HEIGHT * 4);
    display.print(buf);

    // 2nd line
    // speed
    display.setCursor(0, display.height() - CHAR_HEIGHT * 3);
    display.printf("%.1fkmh", satCnt > 0 ? gps.speed.kmph() : 0.0);

    // course
    sprintf(buf, "%.1f'", gps.course.deg());
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2), display.height() - CHAR_HEIGHT * 3);
    display.print(buf);

    // qth
    display.setCursor(display.width() - CHAR_WIDTH * 6, display.height() - CHAR_HEIGHT * 3);
    display.print(isValid ? deg_to_qth(lat, lon) : "------");

    // 3rd line
    display.setCursor(0, display.height() - CHAR_HEIGHT * 2);
    display.print(deg_to_nmea(lat, true));
    display.print(" age ");
    if (isValid) {
        display.print(age / 1000);  // age in seconds
    } else {
        display.print("-");
    }

    // 4th line
    display.setCursor(0, display.height() - CHAR_HEIGHT * 1);
    display.print(deg_to_nmea(lon, false));
    display.print(" dist ");
    if (distance > 99999) {
        display.print("ERR");
    } else {
        display.print(distance);
    }

    OledDrawMsg();
    
    display.display();
#endif
}

void OledUpdateFWU() {
#ifdef USE_SCREEN
    if (AFSK_modem->sending) return;

    char buf[24];

    display.clearDisplay();

    // DateTime
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    sprintf(buf, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2), CHAR_HEIGHT * 2);   // center on the screen
    display.print(buf);

    // Message
    sprintf(buf, "FW Update...");
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2), display.height() - CHAR_HEIGHT * 3);
    display.print(buf);

    display.display();
#endif
}

// add message to show in rectangle box for timeout seconds
void OledPushMsg(String caption, char *msg, char *msg2 = NULL, uint8_t timeout = 3) {
    strncpy(msgBox.caption, caption.c_str(), sizeof(msgBox.caption));
    strncpy(msgBox.msg, msg, sizeof(msgBox.msg));
    if (msg2 != NULL) 
        strncpy(msgBox.msg2, msg2, sizeof(msgBox.msg2));
    else
        memset(msgBox.msg2, 0, sizeof(msgBox.msg2));
    msgBox.timeout = timeout;
}

#if defined(USE_SCREEN_SH1106)
void OledReInit() {
    log_d("OledReInit");
    display.sh1106_command(SH1106_DISPLAYOFF);
    // display.sh1106_command(SH1106_SETDISPLAYCLOCKDIV);
    // display.sh1106_command(0x80);
    // display.sh1106_command(SH1106_SETMULTIPLEX);
    // display.sh1106_command(0x3F);
    display.sh1106_command(SH1106_SETDISPLAYOFFSET);
    display.sh1106_command(0x00);
    display.sh1106_command(SH1106_SETSTARTLINE | 0x00);
    // display.sh1106_command(SH1106_CHARGEPUMP);
    // display.sh1106_command(0x14);
    // display.sh1106_command(SH1106_MEMORYMODE);
    // display.sh1106_command(0x00);
    // display.sh1106_command(SH1106_SEGREMAP | 0x1);
    // display.sh1106_command(SH1106_COMSCANDEC);
    // display.sh1106_command(SH1106_SETCOMPINS);
    // display.sh1106_command(0x12);
    // display.sh1106_command(SH1106_SETCONTRAST);
    // display.sh1106_command(0xCF);
    // display.sh1106_command(SH1106_SETPRECHARGE);
    // display.sh1106_command(0xF1);
    // display.sh1106_command(SH1106_SETVCOMDETECT);
    // display.sh1106_command(0x40);
     // display.sh1106_command(SH1106_DISPLAYALLON_RESUME);
    // display.sh1106_command(SH1106_NORMALDISPLAY);
    display.sh1106_command(SH1106_DISPLAYON);
}
#endif
