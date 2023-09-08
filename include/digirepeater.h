/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the Digipeater functionality.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#ifndef DIGIREPEATER_H
#define DIGIREPEATER_H

#include <AX25.h>

int digiProcess(AX25Msg &Packet);

#endif
