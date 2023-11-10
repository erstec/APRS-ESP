/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the rotary encoder processing.
    Author:         Ernest / LY3PH
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

void onRotaryLeft() {
    //
}

void onRotaryRight() {
    //
}

void onBtnReleased() {
    //
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
