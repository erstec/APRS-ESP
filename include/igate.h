/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the IGate functionality.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#ifndef IGATE_H
#define IGATE_H

#include <LibAPRSesp.h>
#include <WiFiClient.h>
#include <AX25.h>

#include "main.h"

int igateProcess(AX25Msg &Packet);

#endif
