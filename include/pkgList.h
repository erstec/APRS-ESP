/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for various common functions.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#ifndef PKGLIST_H
#define PKGLIST_H

#include <Arduino.h>

typedef struct {
	time_t time;
	char calsign[11];
	char ssid[5];
	bool channel;
	unsigned int pkg;
	uint16_t type;
	uint8_t symbol;
	int16_t audio_level;
	char raw[500];
} pkgListType;

void sort(pkgListType a[], int size);
void sortPkgDesc(pkgListType a[], int size);
int pkgListUpdate(char *call, char *raw, uint16_t type, bool channel);
uint16_t pkgType(const char *raw);
pkgListType getPkgList(int idx);

void pkgListInit();

String pkgGetType(uint32_t type);

#endif // PKGLIST_H
