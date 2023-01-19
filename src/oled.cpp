/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with OLED Display.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "oled.h"
#include "AFSK.h"
#include "TinyGPSPlus.h"
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
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 init failed");
    } else {
        Serial.println("SSD1306 init ok");
    }
#elif defined(USE_SCREEN_SH1106)
    display.begin(SH1106_SWITCHCAPVCC, 0x3C);
#endif

#ifndef NEW_OLED
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
#else
    // Initialising the UI will init the display too.
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(30, 5);
    display.print("ESP32 IGATE");
    display.setCursor(1, 17);
    display.print("Firmware Version " + String(VERSION));
    display.drawLine(10, 30, 110, 30, WHITE);
    display.setCursor(1, 40);
    display.print("Push B Factory reset");
    // topBar(-100);
    display.display();

    delay(1000);
    digitalWrite(LED_TX, HIGH);
    display.setCursor(50, 50);
    display.print("3 Sec");
    display.display();
    delay(1000);
    digitalWrite(LED_RX, HIGH);
    display.setCursor(40, 50);
    display.print("        ");
    display.display();
    display.setCursor(50, 50);
    display.print("2 Sec");
    display.display();
    delay(1000);
    display.setCursor(40, 50);
    display.print("        ");
    display.display();
    display.setCursor(50, 50);
    display.print("1 Sec");
    display.display();
#endif
#endif
}

void OledUpdate() {
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
        display.print("No IP - BLE Mode");
    }

    // Main section
    // Top line
    display.setCursor(0, 0);
    display.printf("%s-%d>%s", config.aprs_mycall, config.aprs_ssid, config.aprs_path);
    for (uint8_t i = display.getCursorX(); i < display.width(); i += CHAR_WIDTH) {
        display.print(" ");
    }

    // Second line
    if (config.aprs) {
        display.setCursor(display.width() - CHAR_WIDTH * 5, CHAR_HEIGHT * 1);
        display.print(aprsClient.connected() ? "A+" : "A-");
    }

    display.setCursor(display.width() - CHAR_WIDTH * 2, CHAR_HEIGHT * 1);
    display.print(WiFi.status() == WL_CONNECTED ? "W+" : "W-");

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
