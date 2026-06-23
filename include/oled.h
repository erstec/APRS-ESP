/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with Configuration, stored in EEPROM.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#pragma once
#include <Arduino.h>

#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8

void OledStartup();
void OledPostStartup(const char* msg);
void OledPostStartup(int step, int total = 4);
void OledUpdate(int batData, bool usbPlugged, bool afskInit, bool charging = false);
void OledUpdateFWU();
void OledPushMsg(String caption, char *msg, char *msg2, uint8_t timeout);
void OledPushMsgDual(const char *line1, const char *line2, uint8_t timeout);
void OledPushMsgBar(const char *caption, char *msg, char *msg2, int8_t bar, uint8_t timeout = 3);
void OledDrawMenu();
void OledTogglePower();
void OledSetBrightness(uint8_t val);
void OledResetActivity();
bool OledIsPopupActive();
bool OledIsOn();
void OledDismissPopup();
void OledEnqueueMsg(String caption, char *msg, char *msg2, uint8_t timeout);
void OledCheckAutoDim();

#define BRIGHTNESS_LEVEL_COUNT 5
extern const uint8_t BRIGHTNESS_LEVELS[BRIGHTNESS_LEVEL_COUNT];
extern const char * const BRIGHTNESS_NAMES[BRIGHTNESS_LEVEL_COUNT];
uint8_t brightnessIdx(uint8_t val);
