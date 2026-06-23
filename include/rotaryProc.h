/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the rotary encoder processing.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
*/

#ifndef ROTARYPROC_H
#define ROTARYPROC_H

#include <Arduino.h>
#include "main.h"

#define MENU_ITEM_COUNT   20

#define MENU_VISIBLE      6   // items shown at once in main menu list
#define QUICK_ITEM_COUNT  6   // 0=Beacon 1=WiFi 2=RF 3=SB 4=Reset 5=Cancel
#define QUICK_PHASE_MS  2000UL // ms each quick item is shown while holding
#define MENU_TIMEOUT_MS 15000UL // ms of inactivity before returning to status screen

// APRS symbol preset table (shared with oled.cpp for display)
struct AprsSymbol {
    const char *name;
    char        table;
    char        symbol;
};
#define APRS_SYMBOL_COUNT 12
extern const AprsSymbol APRS_SYMBOLS[APRS_SYMBOL_COUNT];

enum MenuMode {
    MENU_HIDDEN = 0,    // normal status display
    MENU_MAIN,          // scrollable main menu list
    MENU_QUICK,         // long-press quick-action picker (hold, auto-cycles, release to execute)
    MENU_WIFI_MODE,     // WiFi mode sub-select
    MENU_GPS_MODE,      // GPS mode sub-select
    MENU_SSID_SEL,      // SSID 0-15 select (phase 2 of callsign editor)
    MENU_CALLSIGN,      // callsign character editor
    MENU_SYMBOL,        // APRS symbol preset selector
    MENU_BRIGHTNESS,        // OLED brightness 0-255
    MENU_AUTODIM,           // auto-dim: phase 0=timeout, phase 1=level (two-step)
    MENU_LOCATOR_POP,       // locator change popup duration 0-10s
    MENU_RX_POPUP,          // APRS RX popup duration 0=OFF, 1-10s
    MENU_GNSS,              // live GNSS data overview
    MENU_BATTERY,           // live battery / PMU status
    MENU_ABOUT,             // about / version screen
    MENU_CONFIRM_RESET,     // factory reset confirmation
    MENU_WIFI_REBOOT_CONFIRM, // reboot confirmation after WiFi mode change
    MENU_BEACON,              // beacon mode: SB or fixed interval (minutes)
    MENU_RX_LIST,             // scrollable list of received APRS packets
    MENU_RX_DETAIL,           // detail view of a single received packet
    MENU_MSG_INBOX,           // APRS message inbox list
    MENU_MSG_DETAIL,          // APRS message detail view
};

extern int rxListCount;       // number of valid entries in pkgList (updated by draw)
extern volatile unsigned long lastTxMs;  // millis() of last beacon TX; 0 = never

extern MenuMode       menuMode;
extern int            menuSelected;    // selected item index in MENU_MAIN
extern int            menuScroll;      // first visible item index
extern int            menuSubVal;      // value being edited in sub-menus
extern char           menuCallBuf[10]; // callsign edit buffer
extern int            menuCallPos;     // cursor position in callsign editor
extern int            menuCallCharIdx; // char-set index at current position
extern unsigned long  menuQuickMs;     // timestamp when current quick phase started
extern uint8_t        menuAutodimPhase;   // 0=editing timeout, 1=editing level
extern uint8_t        menuAutodimTimeout; // saved timeout value while editing level

extern const int BEACON_INTERVALS[];
extern const int BEACON_INTERVAL_COUNT;

bool RotaryProcess();

#endif // ROTARYPROC_H
