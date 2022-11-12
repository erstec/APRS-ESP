/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the rotary encoder processing.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
*/

#include "rotaryProc.h"
#include "gps.h"

#ifdef USE_ROTARY
extern Rotary rotary;
#endif

extern bool send_aprs_update;

// aprs symbols
#define APRS_SYM_PHONE      '$'
#define APRS_SYM_CAR        '>'
#define APRS_SYM_BIKE       'b'
#define APRS_SYM_JEEP       'j'
#define APRS_SYM_JOGGER     '['

// aprs ssids
#define APRS_SSID_GENERIC_2         2
#define APRS_SSID_GENERIC_3         3
#define APRS_SSID_WALKIE_TALKIE     7
#define APRS_SSID_PRIMARY_MOBILE    9
#define APRS_SSID_ADDITIONAL        15

#define UPDATE_HEURISTICS_MAP_SIZE  9

char cur_symbol;

void setSymbol(char sym) {
    char ssid = 0;
    switch (sym) {
        case APRS_SYM_JOGGER:
        case APRS_SYM_CAR:
            ssid = APRS_SSID_WALKIE_TALKIE;
            break;
        case APRS_SYM_BIKE:
            ssid = APRS_SSID_GENERIC_3;
            break;
        default:
            ssid = APRS_SYM_JOGGER;
            sym = APRS_SSID_WALKIE_TALKIE;
            break;
    }
    if (ssid != 0) {
        // APRS_setCallsign(APRS_MY_CALLSIGN, ssid);
        // APRS_setSymbol(sym);
        cur_symbol = sym;
        // EEPROM[EEPROM_ACTIVE_SYMBOL_IDX] = sym;
    }
}

void selectNextSymbol() {
    switch (cur_symbol) {
        case APRS_SYM_PHONE:
            setSymbol(APRS_SYM_CAR);
            break;
        case APRS_SYM_CAR:
            setSymbol(APRS_SYM_BIKE);
            break;
        case APRS_SYM_BIKE:
            setSymbol(APRS_SYM_JOGGER);
            break;
        default:
            setSymbol(APRS_SYM_CAR);
            break;
    }
}

void activateAprsUpdateHeuristic(char heuristic) {
    unsigned long timeout = 0L;
    switch (heuristic) {
        case H_OFF:
            // deleteActiveAprsUpdateTimer();
            break;
        case H_PERIODIC_1MIN:
            timeout = 1L * 60L * 1000L;
            break;
        case H_PERIODIC_5MIN:
            timeout = 5L * 60L * 1000L;
            break;
        case H_PERIODIC_10MIN:
            timeout = 10L * 60L * 1000L;
            break;
        case H_PERIODIC_30MIN:
            timeout = 30L * 60L * 1000L;
            break;
        case H_RANGE_500M:
        case H_RANGE_1KM:
        case H_RANGE_5KM:
            distance = 0;
            timeout = 30L * 60L * 1000L;
            break;
        case H_SMART_BEACONING:
            break;
        default:
            break;
    }
    // deleteActiveAprsUpdateTimer();
    if (timeout != 0) {
        // aprs_update_timer_idx = timer.setInterval(timeout, setAprsUpdateFlag);
    }
    active_heuristic = heuristic;
    selected_heuristic = heuristic;
    // EEPROM[EEPROM_ACTIVE_HEURISTIC_IDX] = selected_heuristic;
}

void onRotaryLeft() {
    if (--selected_heuristic < 0) {
        selected_heuristic = UPDATE_HEURISTICS_MAP_SIZE - 1;
    }
}

void onRotaryRight() {
    if (++selected_heuristic >= UPDATE_HEURISTICS_MAP_SIZE) {
        selected_heuristic = 0;
    }
}

void onBtnReleased() {
    // activate selected heuristic
    if (selected_heuristic == active_heuristic) {
        selectNextSymbol();
    } else {
        activateAprsUpdateHeuristic(selected_heuristic);
    }
}

void onLongBtnReleased() {
    send_aprs_update = true;
}

bool RotaryProcess() {
    bool update_screen = false;
#ifdef USE_ROTARY
    unsigned char rotary_state = rotary.process();
    unsigned char rotary_btn_state = rotary.process_button();

    // rotation right-left
    if (rotary_state) {
        if (rotary_state == DIR_CW) {
            onRotaryLeft();
        } else {
            onRotaryRight();
        }
        update_screen = true;
    }

    // button state
    switch (rotary_btn_state) {
        case BTN_NONE:
            break;
        case BTN_PRESSED:
            break;
        case BTN_RELEASED:
            onBtnReleased();
            update_screen = true;
            break;
        case BTN_PRESSED_LONG:
            break;
        case BTN_RELEASED_LONG:
            onLongBtnReleased();
            update_screen = true;
            break;
        default:
            break;
    }
#else
    if (digitalRead(PIN_ROT_BTN) == LOW) {
#ifdef USE_ROTARY
        update_screen = true;
#endif
    }
#endif

    return update_screen;
}
