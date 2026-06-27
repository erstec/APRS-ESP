/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the rotary encoder processing.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
*/

#include "rotaryProc.h"
#include "config.h"
#include "gps.h"
#include "oled.h"
#include "AFSK.h"
#include "aprsMsg.h"
#include <TinyGPS++.h>

extern bool AFSKInitAct;
extern TinyGPSPlus gps;
#ifdef USE_ROTARY
#include <Rotary.h>
#endif

extern Configuration config;

#ifdef USE_ROTARY
extern Rotary rotary;
#endif

extern bool send_aprs_update;
extern long sendTimer;

// ── APRS symbol preset table ──────────────────────────────────────────────────
const AprsSymbol APRS_SYMBOLS[APRS_SYMBOL_COUNT] = {
    {"Car",        '/', '>'},
    {"House",      '/', '-'},
    {"Hiker",      '/', '['},
    {"Bike",       '/', 'b'},
    {"Motorcycle", '/', '<'},
    {"Truck",      '/', 'k'},
    {"Van",        '/', 'v'},
    {"Boat",       '/', 's'},
    {"WX Stn",     '/', '_'},
    {"Balloon",    '/', 'O'},
    {"iGate",      '/', '&'},
    {"Digi",       '\\', '#'},
};

// ── Callsign character set ────────────────────────────────────────────────────
// Index CALL_CHARS_COUNT is the "end" marker (shown as '_', terminates callsign)
static const char CALL_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#define CALL_CHARS_COUNT 36

// ── Menu state (exported via header) ─────────────────────────────────────────
static unsigned long menuLastActivityMs = 0; // updated on any real user input
MenuMode      menuMode        = MENU_HIDDEN;
int           menuSelected    = 0;
int           menuScroll      = 0;
int           menuSubVal      = 0;
char          menuCallBuf[10] = {0};
int           menuCallPos     = 0;
int           menuCallCharIdx = 0;
unsigned long menuQuickMs     = 0;
uint8_t       menuAutodimPhase   = 0;
uint8_t       menuAutodimTimeout = 0;

const int BEACON_INTERVALS[]  = {0, 1, 2, 5, 10, 15, 20, 30, 60};
const int BEACON_INTERVAL_COUNT = 9;
static int    menuBeaconLastFixed = 10; // last fixed interval for SB quick toggle

int rxListCount = 0; // valid pkgList entries; updated by OLED draw, read by CW/CCW clamp

// ── Helpers (only used when rotary is present) ────────────────────────────────
#ifdef USE_ROTARY
static void scrollAdjust() {
    if (menuSelected < menuScroll)
        menuScroll = menuSelected;
    if (menuSelected >= menuScroll + MENU_VISIBLE)
        menuScroll = menuSelected - MENU_VISIBLE + 1;
}

static void callsignApplyChar() {
    if (menuCallCharIdx < CALL_CHARS_COUNT)
        menuCallBuf[menuCallPos] = CALL_CHARS[menuCallCharIdx];
    else
        menuCallBuf[menuCallPos] = 0;  // end-of-callsign marker
}

static int callCharIdxFor(char c) {
    for (int i = 0; i < CALL_CHARS_COUNT; i++)
        if (CALL_CHARS[i] == c) return i;
    return CALL_CHARS_COUNT;  // unknown → end marker
}

// ── Input handlers ────────────────────────────────────────────────────────────
static void onRotaryCW() {   // clockwise = scroll down / increment
    if (OledIsPopupActive()) OledDismissPopup();
    OledResetActivity();
    switch (menuMode) {
        case MENU_MAIN:
            menuSelected = (menuSelected + 1) % MENU_ITEM_COUNT;
            scrollAdjust();
            break;
        case MENU_WIFI_MODE:
            menuSubVal = (menuSubVal + 1) % 4;
            break;
        case MENU_GPS_MODE:
            menuSubVal = (menuSubVal + 1) % 3;
            break;
        case MENU_SSID_SEL:
            menuSubVal = (menuSubVal + 1) % 16;
            break;
        case MENU_CALLSIGN:
            menuCallCharIdx = (menuCallCharIdx + 1) % (CALL_CHARS_COUNT + 1);
            callsignApplyChar();
            break;
        case MENU_SYMBOL:
            menuSubVal = (menuSubVal + 1) % APRS_SYMBOL_COUNT;
            break;
        case MENU_BRIGHTNESS:
            if (menuSubVal < BRIGHTNESS_LEVEL_COUNT - 1)
                OledSetBrightness(BRIGHTNESS_LEVELS[++menuSubVal]);
            break;
        case MENU_AUTODIM:
            if (menuAutodimPhase == 0) { if (menuSubVal < 60) menuSubVal++; }
            else                       { menuSubVal = (menuSubVal + 1) % 5; }
            break;
        case MENU_CONFIRM_RESET:
        case MENU_WIFI_REBOOT_CONFIRM:
            menuSubVal = (menuSubVal + 1) % 2;
            break;
        case MENU_LOCATOR_POP:
            menuSubVal = (menuSubVal + 1) % 11;
            break;
        case MENU_RX_POPUP:
            menuSubVal = (menuSubVal + 1) % 11;
            break;
        case MENU_SMS_POPUP:
            menuSubVal = (menuSubVal + 1) % 4;
            break;
        case MENU_BEACON:
            if (menuSubVal < BEACON_INTERVAL_COUNT - 1) menuSubVal++;
            break;
        case MENU_RX_LIST:
        case MENU_RX_DETAIL:
            if (rxListCount > 0 && menuSubVal < rxListCount - 1) menuSubVal++;
            break;
        case MENU_MSG_INBOX:
        case MENU_MSG_DETAIL:
            if (aprsMsgCount() > 0 && menuSubVal < aprsMsgCount() - 1) menuSubVal++;
            break;
        case MENU_QUICK:
        default: break;
    }
}

static void onRotaryCCW() {  // counter-clockwise = scroll up / decrement
    if (OledIsPopupActive()) OledDismissPopup();
    OledResetActivity();
    switch (menuMode) {
        case MENU_MAIN:
            menuSelected = (menuSelected - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
            scrollAdjust();
            break;
        case MENU_WIFI_MODE:
            menuSubVal = (menuSubVal - 1 + 4) % 4;
            break;
        case MENU_GPS_MODE:
            menuSubVal = (menuSubVal - 1 + 3) % 3;
            break;
        case MENU_SSID_SEL:
            menuSubVal = (menuSubVal - 1 + 16) % 16;
            break;
        case MENU_CALLSIGN:
            menuCallCharIdx = (menuCallCharIdx - 1 + CALL_CHARS_COUNT + 1) % (CALL_CHARS_COUNT + 1);
            callsignApplyChar();
            break;
        case MENU_SYMBOL:
            menuSubVal = (menuSubVal - 1 + APRS_SYMBOL_COUNT) % APRS_SYMBOL_COUNT;
            break;
        case MENU_BRIGHTNESS:
            if (menuSubVal > 0)
                OledSetBrightness(BRIGHTNESS_LEVELS[--menuSubVal]);
            break;
        case MENU_AUTODIM:
            if (menuAutodimPhase == 0) { if (menuSubVal > 0) menuSubVal--; }
            else                       { menuSubVal = (menuSubVal - 1 + 5) % 5; }
            break;
        case MENU_CONFIRM_RESET:
        case MENU_WIFI_REBOOT_CONFIRM:
            menuSubVal = (menuSubVal + 1) % 2;
            break;
        case MENU_LOCATOR_POP:
            menuSubVal = (menuSubVal - 1 + 11) % 11;
            break;
        case MENU_RX_POPUP:
            menuSubVal = (menuSubVal - 1 + 11) % 11;
            break;
        case MENU_SMS_POPUP:
            menuSubVal = (menuSubVal - 1 + 4) % 4;
            break;
        case MENU_BEACON:
            if (menuSubVal > 0) menuSubVal--;
            break;
        case MENU_RX_LIST:
        case MENU_RX_DETAIL:
            if (menuSubVal > 0) menuSubVal--;
            break;
        case MENU_MSG_INBOX:
        case MENU_MSG_DETAIL:
            if (menuSubVal > 0) menuSubVal--;
            break;
        case MENU_QUICK:
        default: break;
    }
}

static void onBtnReleased() {
    log_i("[BTN] {\"btn\":\"SW2\",\"type\":\"short\"}");
    if (OledIsPopupActive()) { OledDismissPopup(); return; }
    OledResetActivity();
    switch (menuMode) {
        case MENU_HIDDEN:
            menuMode = MENU_MAIN;   // short press = enter menu
            break;

        case MENU_MAIN:
            switch (menuSelected) {
                case 0: {  // Send Beacon
                    char empty[] = "";
                    if (!config.tnc) {
                        char msg[] = "RF IS OFF";
                        OledPushMsg("", msg, empty, 3);
                    } else if (config.gps_mode == GPS_MODE_GPS && !gps.location.isValid()) {
                        char msg[] = "NO GPS FIX";
                        OledPushMsg("", msg, empty, 3);
                    } else if (lastTxMs > 0 && millis() - lastTxMs < 30000UL) {
                        char waitMsg[16];
                        char tooFast[] = "Too fast!";
                        snprintf(waitMsg, sizeof(waitMsg), "Wait %ds!", (int)((30000UL - (millis() - lastTxMs) + 999) / 1000));
                        OledPushMsg("", tooFast, waitMsg, 3);
                    } else {
                        menuMode = MENU_HIDDEN;  // must precede flag: OledPushMsg I2C takes ~25ms during which taskAPRS reads the flag
                        send_aprs_update = true;
                        char msg[] = "Beacon TX";
                        OledPushMsg("", msg, empty, 1);
                    }
                    menuMode = MENU_HIDDEN;
                    break;
                }
                case 1: {  // Beacon Mode
                    menuSubVal = 4;  // default: 10 min
                    for (int i = 0; i < BEACON_INTERVAL_COUNT; i++) {
                        if (BEACON_INTERVALS[i] == config.aprs_beacon) { menuSubVal = i; break; }
                    }
                    menuMode = MENU_BEACON;
                    break;
                }
                case 2:  // WiFi Mode
                    menuSubVal = config.wifi_mode;
                    menuMode = MENU_WIFI_MODE;
                    break;
                case 3:  // APRS-IS
                    config.aprs = !config.aprs;
                    SaveConfig();
                    break;
                case 4:  // Digipeater
                    config.tnc_digi = !config.tnc_digi;
                    SaveConfig();
                    break;
                case 5:  // RF/TNC
                    config.tnc = !config.tnc;
                    SaveConfig();
                    break;
                case 6:  // RF Power
                    config.rf_power = !config.rf_power;
                    SaveConfig();
#ifdef POWER_PIN
                    digitalWrite(POWER_PIN, config.rf_power);
#endif
                    break;
                case 7:  // GPS Mode
                    menuSubVal = config.gps_mode;
                    menuMode = MENU_GPS_MODE;
                    break;
                case 8:  // Callsign-SSID
                    strlcpy(menuCallBuf, config.aprs_mycall, sizeof(menuCallBuf));
                    menuCallPos = 0;
                    menuCallCharIdx = callCharIdxFor(menuCallBuf[0]);
                    menuSubVal = config.aprs_ssid;
                    menuMode = MENU_CALLSIGN;
                    break;
                case 9: {  // Symbol
                    menuSubVal = 0;
                    for (int i = 0; i < APRS_SYMBOL_COUNT; i++) {
                        if (APRS_SYMBOLS[i].table  == config.aprs_table &&
                            APRS_SYMBOLS[i].symbol == config.aprs_symbol) {
                            menuSubVal = i; break;
                        }
                    }
                    menuMode = MENU_SYMBOL;
                    break;
                }
                case 10:  // Brightness
                    menuSubVal = brightnessIdx(config.oled_brightness);
                    menuMode = MENU_BRIGHTNESS;
                    break;
                case 11:  // Auto-Dim (timeout + level, two phases)
                    menuAutodimPhase = 0;
                    menuSubVal = config.oled_autodim;
                    menuMode = MENU_AUTODIM;
                    break;
                case 12:  // Locator Popup
                    menuSubVal = config.locator_popup;
                    menuMode = MENU_LOCATOR_POP;
                    break;
                case 13:  // RX Popup
                    menuSubVal = config.aprs_rx_popup;
                    menuMode = MENU_RX_POPUP;
                    break;
                case 14: {  // SMS Popup
                    static const uint8_t SMS_POPUP_VALS[] = {0, 15, 30, 60};
                    menuSubVal = 0;
                    for (int i = 0; i < 4; i++) {
                        if (SMS_POPUP_VALS[i] == config.aprs_sms_popup) { menuSubVal = i; break; }
                    }
                    menuMode = MENU_SMS_POPUP;
                    break;
                }
                case 15:  // RX Packet List
                    menuSubVal = 0;
                    menuMode = MENU_RX_LIST;
                    break;
                case 16:  // MSG Inbox
                    menuSubVal = 0;
                    menuMode = MENU_MSG_INBOX;
                    break;
                case 17:  // GNSS Status
                    menuMode = MENU_GNSS;
                    break;
                case 18:  // Battery Status
                    menuMode = MENU_BATTERY;
                    break;
                case 19:  // About
                    menuMode = MENU_ABOUT;
                    break;
                case 20:  // Factory Reset
                    menuSubVal = 0;  // default: NO
                    menuMode = MENU_CONFIRM_RESET;
                    break;
            }
            break;

        case MENU_BRIGHTNESS:
            config.oled_brightness = BRIGHTNESS_LEVELS[menuSubVal];
            SaveConfig();
            menuMode = MENU_MAIN;
            break;

        case MENU_WIFI_MODE:
            config.wifi_mode = (uint8_t)menuSubVal;
            SaveConfig();
            menuSubVal = 0;  // default: YES (reboot now)
            menuMode = MENU_WIFI_REBOOT_CONFIRM;
            break;

        case MENU_WIFI_REBOOT_CONFIRM:
            if (menuSubVal == 0) {  // YES — reboot now
                char msg[] = "Rebooting...";
                char empty[] = "";
                OledPushMsg("", msg, empty, 2);
                delay(2000);
                ESP.restart();
            } else {  // NO — will apply on next reboot
                char msg[] = "Reboot to";
                char msg2[] = "apply changes";
                OledPushMsg("", msg, msg2, 3);
                menuMode = MENU_MAIN;
            }
            break;

        case MENU_AUTODIM:
            if (menuAutodimPhase == 0) {
                menuAutodimTimeout = (uint8_t)menuSubVal;
                menuSubVal = config.oled_autodim_level;
                menuAutodimPhase = 1;
            } else {
                config.oled_autodim       = menuAutodimTimeout;
                config.oled_autodim_level = (uint8_t)menuSubVal;
                SaveConfig();
                menuMode = MENU_MAIN;
            }
            break;

        case MENU_BEACON:
            config.aprs_beacon = BEACON_INTERVALS[menuSubVal];
            if (config.aprs_beacon > 0) menuBeaconLastFixed = config.aprs_beacon;
            SaveConfig();
            sendTimer = millis();
            menuMode = MENU_MAIN;
            break;

        case MENU_LOCATOR_POP:
            config.locator_popup = (uint8_t)menuSubVal;
            SaveConfig();
            menuMode = MENU_MAIN;
            break;

        case MENU_RX_POPUP:
            config.aprs_rx_popup = (uint8_t)menuSubVal;
            SaveConfig();
            menuMode = MENU_MAIN;
            break;

        case MENU_SMS_POPUP: {
            static const uint8_t SMS_POPUP_VALS[] = {0, 15, 30, 60};
            config.aprs_sms_popup = SMS_POPUP_VALS[menuSubVal % 4];
            SaveConfig();
            menuMode = MENU_MAIN;
            break;
        }

        case MENU_GPS_MODE:
            config.gps_mode = (uint8_t)menuSubVal;
            SaveConfig();
            menuMode = MENU_MAIN;
            break;

        case MENU_SSID_SEL:
            strlcpy(config.aprs_mycall, menuCallBuf, sizeof(config.aprs_mycall));
            config.aprs_ssid = (uint8_t)menuSubVal;
            SaveConfig();
            menuMode = MENU_MAIN;
            break;

        case MENU_SYMBOL:
            config.aprs_table  = APRS_SYMBOLS[menuSubVal].table;
            config.aprs_symbol = APRS_SYMBOLS[menuSubVal].symbol;
            SaveConfig();
            menuMode = MENU_MAIN;
            break;

        case MENU_CALLSIGN:
            if (menuCallCharIdx == CALL_CHARS_COUNT && menuCallPos >= 3) {
                // end marker + min 3 chars met → move to SSID phase
                menuCallBuf[menuCallPos] = 0;
                menuMode = MENU_SSID_SEL;
            } else if (menuCallPos >= 5) {
                // max 6 chars (AX.25) → move to SSID phase
                menuCallBuf[6] = 0;
                menuMode = MENU_SSID_SEL;
            } else if (menuCallCharIdx != CALL_CHARS_COUNT) {
                // advance cursor (end marker ignored below min length)
                menuCallPos++;
                menuCallCharIdx = callCharIdxFor(menuCallBuf[menuCallPos]);
            }
            break;

        case MENU_CONFIRM_RESET:
            if (menuSubVal == 1) {  // YES
                DefaultConfig();
                SaveConfig();
                delay(300);
                ESP.restart();
            } else {
                menuMode = MENU_MAIN;
            }
            break;

        case MENU_GNSS:
            menuMode = MENU_MAIN;
            break;

        case MENU_BATTERY:
            menuMode = MENU_MAIN;
            break;

        case MENU_ABOUT:
            menuMode = MENU_MAIN;
            break;

        case MENU_RX_LIST:
            if (rxListCount > 0)
                menuMode = MENU_RX_DETAIL;
            break;

        case MENU_RX_DETAIL:
            menuMode = MENU_RX_LIST;
            break;

        case MENU_MSG_INBOX:
            if (aprsMsgCount() > 0)
                menuMode = MENU_MSG_DETAIL;
            break;

        case MENU_MSG_DETAIL:
            menuMode = MENU_MSG_INBOX;
            break;

        case MENU_QUICK:
            break;  // ignore press while in quick picker (release executes)
    }
}

static void onBtnPressedLong() {
    log_i("[BTN] {\"btn\":\"SW2\",\"type\":\"long\"}");
    if (OledIsPopupActive()) { OledDismissPopup(); return; }
    switch (menuMode) {
        case MENU_HIDDEN:
            // enter quick-action picker from status screen only
            menuMode    = MENU_QUICK;
            menuSubVal  = 0;
            menuQuickMs = millis();
            break;
        case MENU_BRIGHTNESS:
            OledSetBrightness(config.oled_brightness);  // restore on cancel
            menuMode = MENU_MAIN;
            break;
        case MENU_CALLSIGN:
            strlcpy(menuCallBuf, config.aprs_mycall, sizeof(menuCallBuf));
            menuMode = MENU_MAIN;
            break;
        default:
            // long press from any menu = exit to status screen
            menuMode = MENU_HIDDEN;
            break;
    }
}

static void onBtnReleasedLong() {
    if (menuMode != MENU_QUICK) { menuMode = MENU_HIDDEN; return; }
    char empty[] = "";
    switch (menuSubVal) {
        case 0: {  // Send Beacon
            if (!config.tnc) {
                char msg[] = "RF IS OFF";
                OledPushMsg("", msg, empty, 3);
            } else if (config.gps_mode == GPS_MODE_GPS && !gps.location.isValid()) {
                char msg[] = "NO GPS FIX";
                OledPushMsg("", msg, empty, 3);
            } else if (lastTxMs > 0 && millis() - lastTxMs < 30000UL) {
                char waitMsg[16];
                char tooFast[] = "Too fast!";
                snprintf(waitMsg, sizeof(waitMsg), "Wait %ds!", (int)((30000UL - (millis() - lastTxMs) + 999) / 1000));
                OledPushMsg("", tooFast, waitMsg, 3);
            } else {
                menuMode = MENU_HIDDEN;  // must precede flag: OledPushMsg I2C takes ~25ms during which taskAPRS reads the flag
                send_aprs_update = true;
                char msg[] = "Beacon TX";
                OledPushMsg("", msg, empty, 1);
            }
            break;
        }
        case 1: {  // WiFi SW
            config.wifi_mode = (config.wifi_mode == WIFI_OFF_FIX) ? WIFI_AP_STA_FIX : WIFI_OFF_FIX;
            SaveConfig();
            char msg[] = "WiFi ON";
            char msgOff[] = "WiFi OFF";
            OledPushMsg("", (config.wifi_mode == WIFI_OFF_FIX) ? msgOff : msg, empty, 3);
            delay(1500);
            ESP.restart();
            break;
        }
        case 2: {  // RF SW
            config.tnc = !config.tnc;
            SaveConfig();
            char msgOn[] = "RF ON";
            char msgOff[] = "RF OFF";
            OledPushMsg("", config.tnc ? msgOn : msgOff, empty, 3);
            break;
        }
        case 3: {  // SmartBeaconing toggle
            if (config.aprs_beacon == 0) {
                config.aprs_beacon = menuBeaconLastFixed > 0 ? menuBeaconLastFixed : 10;
            } else {
                menuBeaconLastFixed = config.aprs_beacon;
                config.aprs_beacon = 0;
            }
            SaveConfig();
            sendTimer = millis();
            char msgSB[]  = "SB ON";
            char msgFix[] = "SB OFF";
            OledPushMsg("", config.aprs_beacon == 0 ? msgSB : msgFix, empty, 3);
            break;
        }
        case 4:  // Factory Reset — ask for confirmation
            menuSubVal = 0;  // default: NO
            menuMode = MENU_CONFIRM_RESET;
            return;  // skip menuMode = MENU_HIDDEN below
        case 5:  // Cancel
        default: {
            char msg[] = "Cancel";
            OledPushMsg("", msg, empty, 2);
            break;
        }
    }
    menuMode = MENU_HIDDEN;
}
#endif  // USE_ROTARY

// ── RotaryProcess ─────────────────────────────────────────────────────────────
bool RotaryProcess() {
    bool update_screen = false;
#ifdef USE_ROTARY
#ifndef I2S_INTERNAL
    MenuMode prevMode = menuMode;
#endif
    unsigned char rotary_state     = rotary.consumePending();
    unsigned char rotary_btn_state = rotary.process_button();

    if (!OledIsOn()) return false;  // screen off — drain state but ignore all input

    // Inactivity timeout — return to status screen (live view screens are exempt)
    if (menuMode != MENU_HIDDEN && menuMode != MENU_GNSS && menuMode != MENU_BATTERY && menuLastActivityMs > 0 &&
        millis() - menuLastActivityMs >= MENU_TIMEOUT_MS) {
        menuMode = MENU_HIDDEN;
        menuLastActivityMs = 0;
        return true;
    }

    if (rotary_state) {
        menuLastActivityMs = millis();
        if (rotary_state == DIR_CW) {
            onRotaryCW();
        } else {
            onRotaryCCW();
        }
        update_screen = true;
    }

    // Auto-advance quick-action picker while button is held
    if (menuMode == MENU_QUICK && menuSubVal < QUICK_ITEM_COUNT - 1) {
        if (millis() - menuQuickMs >= QUICK_PHASE_MS) {
            menuSubVal++;
            menuQuickMs = millis();
            update_screen = true;
        }
    }

    switch (rotary_btn_state) {
        case BTN_RELEASED:
            menuLastActivityMs = millis();
            onBtnReleased();
            update_screen = true;
            break;
        case BTN_PRESSED_LONG:
            menuLastActivityMs = millis();
            onBtnPressedLong();
            update_screen = true;
            break;
        case BTN_RELEASED_LONG:
            menuLastActivityMs = millis();
            onBtnReleasedLong();
            update_screen = true;
            break;
        default:
            break;
    }

    // Pause/resume AFSK when entering/leaving any menu
#ifndef I2S_INTERNAL
    if (prevMode == MENU_HIDDEN && menuMode != MENU_HIDDEN) {
        if (AFSKInitAct) AFSK_TimerEnable(false);
    } else if (prevMode != MENU_HIDDEN && menuMode == MENU_HIDDEN) {
        if (AFSKInitAct) AFSK_TimerEnable(true);
    }
#endif

#else
    if (digitalRead(PIN_ROT_BTN) == LOW) {
        update_screen = true;
    }
#endif

    return update_screen;
}
