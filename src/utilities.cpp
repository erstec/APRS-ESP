/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for various common functions.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "utilities.h"

String getValue(String data, char separator, int index) {
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length();

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}  // END

boolean isValidNumber(String str) {
    for (byte i = 0; i < str.length(); i++) {
        if (isDigit(str.charAt(i))) return true;
    }
    return false;
}

void printTime() {
    char buf[3];
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    Serial.print("[");
    sprintf(buf, "%02d", tmstruct.tm_hour);
    Serial.print(buf);
    Serial.print(":");
    sprintf(buf, "%02d", tmstruct.tm_min);
    Serial.print(buf);
    Serial.print(":");
    sprintf(buf, "%02d", tmstruct.tm_sec);
    Serial.print(buf);
    Serial.print("] ");
}
