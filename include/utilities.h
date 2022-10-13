/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for various common functions.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>

String getValue(String data, char separator, int index);
boolean isValidNumber(String str);
void printTime();

#endif // HELPERS_H
