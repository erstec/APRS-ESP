/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for various common functions.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>
#include <time.h>

String getValue(String data, char separator, int index);
boolean isValidNumber(String str);
void printTime(char *buf);

#endif // HELPERS_H
