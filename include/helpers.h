#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>

String getValue(String data, char separator, int index);
boolean isValidNumber(String str);
uint8_t checkSum(uint8_t *ptr, size_t count);
void printTime();

#endif // HELPERS_H
