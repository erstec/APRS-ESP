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

#if defined(USE_SCREEN_SSD1306)
#include <Adafruit_SSD1306.h>
extern Adafruit_SSD1306 display;
#elif defined(USE_SCREEN_SH1106)
#include <Adafruit_SH1106.h>
extern Adafruit_SH1106 display;
#endif

extern TinyGPSPlus gps;
extern Configuration config;
extern WiFiClient aprsClient;

void OledStartup() {
#ifdef USE_SCREEN
#if defined(USE_SCREEN_SSD1306)
    // Explicit Wire pins assignment
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 init failed");
    } else {
        Serial.println("SSD1306 init ok");
    }
#elif defined(USE_SCREEN_SH1106)
    display.begin(SH1106_SWITCHCAPVCC, 0x3C);
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
    display.println(buf);
    display.print("RF init...");

    display.display();
#endif
}

void OledUpdate(int batData, bool usbPlugged) {
#ifdef USE_SCREEN
    if (AFSK_modem->sending) return;

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

    // DateTime / Battey
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    sprintf(buf, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2) , CHAR_HEIGHT * 2);   // center on the screen
    display.print(buf);
    // Timesync source
    display.setCursor((display.width() / 2) + (strlen(buf) * CHAR_WIDTH / 2) + CHAR_WIDTH, CHAR_HEIGHT * 2);
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
    for (uint8_t i = display.getCursorX(); i < display.width(); i += CHAR_WIDTH) {
        display.print(" ");
    }

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
    for (uint8_t i = display.getCursorX(); i < display.width(); i += CHAR_WIDTH) {
        display.print(" ");
    }

    // altitude
    sprintf(buf, "%.1fm", gps.altitude.meters());
    display.setCursor(display.width() - CHAR_WIDTH * strlen(buf), display.height() - CHAR_HEIGHT * 4);
    display.print(buf);

    // 2nd line
    // speed
    display.setCursor(0, display.height() - CHAR_HEIGHT * 3);
    display.printf("%.1fkmh", satCnt > 0 ? gps.speed.kmph() : 0.0);
    for (uint8_t i = display.getCursorX(); i < display.width(); i += CHAR_WIDTH) {
        display.print(" ");
    }

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
    for (uint8_t i = display.getCursorX(); i < display.width(); i += CHAR_WIDTH) {
        display.print(" ");
    }

    // 4th line
    display.setCursor(0, display.height() - CHAR_HEIGHT * 1);
    display.print(deg_to_nmea(lon, false));
    display.print(" dist ");
    display.print(distance);
    for (uint8_t i = display.getCursorX(); i < display.width(); i += CHAR_WIDTH) {
        display.print(" ");
    }
    
    display.display();
#endif

//     // // active mode / selected mode / free memory
//     // displayPrintPgm(
//     //     (char *)pgm_read_word(&(update_heuristics_map[active_heuristic])));
//     // display.print(F("/"));
//     // displayPrintPgm(
//     //     (char *)pgm_read_word(&(update_heuristics_map[selected_heuristic])));
//     // display.print(F("/"));
//     // display.print(freeMemory());
//     // display.print(F("/"));
//     // display.println((char)cur_symbol);

//     // count updates / count tx / count rx / satellites / age
//     // display.print(cnt);
//     // display.print(F("/"));
//     // display.print(cnt_tx);
//     // display.print(F("tx"));
//     // display.print(F("/"));
//     // display.print(cnt_rx);
//     // display.println(F("rx"));
//
//     display.display();
// #endif
//     // cnt++;
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
