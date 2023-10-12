/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the RF modem control.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate                    
*/

#ifndef RF_MODEM_H
#define RF_MODEM_H

#include <Arduino.h>

bool RF_Init(bool boot);
void RF_Check();

#endif // RF_MODEM_H
