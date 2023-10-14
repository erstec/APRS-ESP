/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the GPS processing.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
*/

#include "gps.h"
#include "smartBeaconing.h"
#include "TinyGPS++.h"
#include "config.h"

// #define DEBUG_GPS

// extern Configuration config;
extern TinyGPSPlus gps;
extern HardwareSerial SerialGPS;
extern Configuration config;

extern teTimeSync timeSyncFlag;

// buffer for conversions
#define CONV_BUF_SIZE 15
static char conv_buf[CONV_BUF_SIZE];

int8_t active_heuristic = H_OFF;
int8_t selected_heuristic = H_OFF;

// static bool gotGpsFix = false;

#define USE_PRECISE_DISTANCE
#define GPS_POLL_DURATION_MS 1000  // for how long to poll gps

long lat = 0;
long lon = 0;
long lat_prev = 0;
long lon_prev = 0;
long distance = 0;
unsigned long age = 0;

bool send_aprs_update = false;

void heuristicDistanceChanged() {
    bool needs_update = false;
    switch (active_heuristic) {
        case H_RANGE_500M:
            if (distance > 500) {
                needs_update = true;
            }
            break;
        case H_RANGE_1KM:
            if (distance > 1000) {
                needs_update = true;
            }
            break;
        case H_RANGE_5KM:
            if (distance > 5000) {
                needs_update = true;
            }
            break;
        default:
            needs_update = SmartBeaconingProc();
            break;
    }
    if (needs_update) {
        send_aprs_update = true;
#ifndef USE_PRECISE_DISTANCE
        lon_prev = lon;
        lat_prev = lat;
#endif
        distance = 0;
    }
}

long distanceBetween(long lat1, long long1, long lat2, long long2) {
    return gps.distanceBetween(
        ((float)lat1) / 1000000.0, ((float)long1) / 1000000.0,
        ((float)lat2) / 1000000.0, ((float)long2) / 1000000.0);
}

void updateDistance() {
    if (lon_prev != 0 && lat_prev != 0 && lon != 0 && lat != 0) {
#ifdef USE_GPS
        distance += distanceBetween(lon_prev, lat_prev, lon, lat);
#endif
        // heuristicDistanceChanged();
    }
#ifndef USE_PRECISE_DISTANCE
    else {
#endif
        lon_prev = lon;
        lat_prev = lat;
#ifndef USE_PRECISE_DISTANCE
    }
#endif
}

void GpsUpdate() {
#ifdef USE_GPS
    //     for (unsigned long start = millis(); millis() - start <
    //     GPS_POLL_DURATION_MS;)
    //     {
    while (SerialGPS.available()) {
        char c = SerialGPS.read();
#ifdef DEBUG_GPS
        Serial.print(c);  // Debug
#endif
        if (gps.encode(c)) {
            lat = gps.location.lat() * 1000000;
            lon = gps.location.lng() * 1000000;
            age = gps.location.age();

            if (config.synctime && gps.time.isValid() && timeSyncFlag == T_SYNC_NONE) {
                Serial.print("GPS Time: ");
                Serial.print(gps.time.hour());
                Serial.print(":");
                Serial.print(gps.time.minute());
                Serial.print(":");
                Serial.println(gps.time.second());

                // time_t systemTime;
                // time(&systemTime);
                // setTime(systemTime);

                timeSyncFlag = T_SYNC_GPS;
            }

            updateDistance();
            // if (!gotGpsFix && gps.location.isValid()) gotGpsFix = true;
        }

        // if (millis() - start >= GPS_POLL_DURATION_MS)
        //     break;
    }
    // yield();
//     }
#endif
}

float conv_coords(float in_coords) {
    // Initialize the location.
    float f = in_coords;
    // Get the first two digits by turning f into an integer, then doing an
    // integer divide by 100; firsttowdigits should be 77 at this point.
    int firsttwodigits = ((int)f) / 100;  // This assumes that f < 10000.
    float nexttwodigits = f - (float)(firsttwodigits * 100);
    float theFinalAnswer = (float)(firsttwodigits + nexttwodigits / 60.0);
    return theFinalAnswer;
}

void DD_DDDDDtoDDMMSS(float DD_DDDDD, int *DD, int *MM, int *SS) {
    *DD = (int)DD_DDDDD;  //сделали из 37.45545 это 37 т.е. Градусы
    *MM = (int)((DD_DDDDD - *DD) * 60);         //получили минуты
    *SS = ((DD_DDDDD - *DD) * 60 - *MM) * 100;  //получили секунды
}

/*
**  Convert degrees in long format to NMEA string format
**  DDMM.hhN for latitude and DDDMM.hhW for longitude
*/
char *deg_to_nmea(long deg, boolean is_lat) {
    unsigned long b = (deg % 1000000UL) * 60UL;
    unsigned long a = (deg / 1000000UL) * 100UL + b / 1000000UL;
    b = (b % 1000000UL) / 10000UL;
    // DDMM.hhN/DDDMM.hhW
    // http://www.aprs.net/vm/DOS/PROTOCOL.HTM
    conv_buf[0] = '0';
    snprintf(conv_buf + 1, 5, "%04u", a);
    conv_buf[5] = '.';
    snprintf(conv_buf + 6, 3, "%02u", b);
    conv_buf[9] = '\0';
    if (is_lat) {
        conv_buf[8] = deg > 0 ? 'N' : 'S';
        return conv_buf + 1;
    } else {
        conv_buf[8] = deg > 0 ? 'E' : 'W';
        return conv_buf;
    }
}

/*
**  Convert latidude and longitude to QTH maidenhead locator
*/
char *deg_to_qth(long lat, long lon) {
    // https://en.wikipedia.org/wiki/Maidenhead_Locator_System
    float lat_ = ((float)lat) / 1000000.0 + 90.0;
    float lon_ = ((float)lon) / 1000000.0 + 180.0;
    conv_buf[0] = 'A' + int(lon_ / 20.0);
    conv_buf[1] = 'A' + int(lat_ / 10.0);
    conv_buf[2] = '0' + int(fmod(lon_, 20) / 2.0);
    conv_buf[3] = '0' + int(fmod(lat_, 10) / 1.0);
    conv_buf[4] = 'A' + int((lon_ - (int(lon_ / 2.0) * 2)) / (5.0 / 60.0));
    conv_buf[5] = 'A' + int((lat_ - (int(lat_ / 1.0) * 1)) / (2.5 / 60.0));
    conv_buf[6] = '\0';
    return conv_buf;
}
