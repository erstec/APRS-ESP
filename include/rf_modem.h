#ifndef RF_MODEM_H
#define RF_MODEM_H

#include <Arduino.h>

void RF_Init(bool boot);
void RF_Sleep();
void RF_Check();

#endif // RF_MODEM_H
