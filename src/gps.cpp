#include "gps.h"
#include "TinyGPSPlus.h"

// #define DEBUG_GPS

// extern Configuration config;
extern TinyGPSPlus gps;
extern HardwareSerial SerialGPS;

char active_heuristic = H_OFF;
char selected_heuristic = H_OFF;

// static bool gotGpsFix = false;

#define USE_PRECISE_DISTANCE
#define GPS_POLL_DURATION_MS 1000  // for how long to poll gps
//#define USE_SMART_BEACONING

long lat = 0;
long lon = 0;
long lat_prev = 0;
long lon_prev = 0;
long distance = 0;
unsigned long age = 0;

bool send_aprs_update = false;

void heuristicProcessSmartBeaconing() {
#ifdef USE_SMART_BEACONING

    // https://github.com/ge0rg/aprsdroid/blob/master/src/location/SmartBeaconing.scala

    float speed = gps.f_speed_kmph();
    float course_prev = gps.course();
    float heading_change_since_beacon = abs(course_prev - gps.course());
    long beacon_rate, secs_since_beacon, turn_time;

    if (speed < APRS_SB_LOW_SPEED) {
        beacon_rate = APRS_SB_SLOW_RATE;
    } else {
        // Adjust beacon rate according to speed
        if (speed > APRS_SB_HIGH_SPEED) {
            beacon_rate = APRS_SB_FAST_RATE;
        } else {
            beacon_rate = APRS_SB_FAST_RATE * APRS_SB_HIGH_SPEED / speed;
        }

        // Corner pegging - ALWAYS occurs if not "stopped"
        // Note turn threshold is speed-dependent

        // what is mph?
        float turn_threshold =
            APRS_SB_TURN_MIN + APRS_SB_TURN_SLOPE / (speed * 2.23693629);

        if ((heading_change_since_beacon > APRS_SB_TURN_TH) &&
            (secs_since_beacon > turn_time)) {
            secs_since_beacon = beacon_rate;
        }
    }

    if (secs_since_beacon > beacon_rate) {
        send_aprs_update = true;
    }
#endif
}

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
            heuristicProcessSmartBeaconing();
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
        heuristicDistanceChanged();
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

void updateGpsData() {
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