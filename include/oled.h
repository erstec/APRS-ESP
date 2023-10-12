/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with Configuration, stored in EEPROM.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include <Arduino.h>

#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8

void OledStartup();
void OledUpdate(int batData, bool usbPlugged);
void OledUpdateFWU();
