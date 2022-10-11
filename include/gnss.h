#ifndef GNSS_H
#define GNSS_H

#include <Arduino.h>
#include "main.h"

extern long lat;
extern long lon;
extern long lat_prev;
extern long lon_prev;
extern long distance;
extern unsigned long age;

void updateGpsData();

#endif