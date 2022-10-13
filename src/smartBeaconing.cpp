/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the SmartBeaconing.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
                    https://github.com/ge0rg/aprsdroid/blob/master/src/location/SmartBeaconing.scala
*/

#include "smartBeaconing.h"
#include "TinyGPSPlus.h"
#include "main.h"

// smart beaconing parameters
#define APRS_SB_LOW_SPEED   5
#define APRS_SB_HIGH_SPEED  100
#define APRS_SB_SLOW_RATE   1200
#define APRS_SB_FAST_RATE   60
#define APRS_SB_TURN_MIN    15
#define APRS_SB_TURN_TH     10
#define APRS_SB_TURN_SLOPE  240

extern TinyGPSPlus gps;
extern bool send_aprs_update;

void SmartBeaconingProc() {
#ifdef USE_SMART_BEACONING

    // https://github.com/ge0rg/aprsdroid/blob/master/src/location/SmartBeaconing.scala

    float speed = gps.speed.kmph();
    float course_prev = gps.course.deg();
    float heading_change_since_beacon = abs(course_prev - gps.course.deg());
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
        float turn_threshold = APRS_SB_TURN_MIN + APRS_SB_TURN_SLOPE / (speed * 2.23693629);

        if ((heading_change_since_beacon > APRS_SB_TURN_TH) && (secs_since_beacon > turn_time)) {
            secs_since_beacon = beacon_rate;
        }
    }

    if (secs_since_beacon > beacon_rate) {
        send_aprs_update = true;
    }
#endif
}
