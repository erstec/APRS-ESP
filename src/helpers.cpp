#include "helpers.h"

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

uint8_t checkSum(uint8_t *ptr, size_t count) {
    uint8_t lrc, tmp;
    uint16_t i;
    lrc = 0;
    for (i = 0; i < count; i++) {
        tmp = *ptr++;
        lrc = lrc ^ tmp;
    }
    return lrc;
}

void printTime() {
    char buf[3];
    struct tm tmstruct;
    getLocalTime(&tmstruct, 5000);
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
