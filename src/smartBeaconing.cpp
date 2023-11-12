/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the SmartBeaconing.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
                    https://github.com/ge0rg/aprsdroid/blob/master/src/location/SmartBeaconing.scala
    Aknowlegments:  SmartBeaconing™ is a beaconing algorithm invented by Tony Arnerich KD7TA and Steve Bragg KA9MVA.
*/

#include "smartBeaconing.h"
#include "TinyGPS++.h"
#include "config.h"
#include "main.h"

typedef struct {
    bool valid;
    TinyGPSLocation location;
    TinyGPSCourse course;
    TinyGPSSpeed speed;
    TinyGPSTime time;
} Location;

extern TinyGPSPlus gps;
extern Configuration config;

static Location lastLoc = { .valid = false };

int smartBeaconSpeedRate(float speed) {
    float SB_FAST_SPEED = config.sb_fast_speed / 3.6; // [m/s]
    int SB_FAST_RATE = config.sb_fast_rate;
    float SB_SLOW_SPEED = config.sb_slow_speed / 3.6; // [m/s]
    int SB_SLOW_RATE = config.sb_slow_rate;
    if (speed <= SB_SLOW_SPEED)
        return SB_SLOW_RATE;
    else if (speed >= SB_FAST_SPEED)
        return SB_FAST_RATE;
    else
        return (int)(SB_FAST_RATE + (SB_SLOW_RATE - SB_FAST_RATE) * (SB_FAST_SPEED - speed) / (SB_FAST_SPEED-SB_SLOW_SPEED));
}

// returns the angle between two bearings
float getBearingAngle(float alpha, float beta) {
    float delta = fmod(abs(alpha - beta), 360);
    if (delta <= 180) 
        return delta;
    else 
        return (360 - delta);
}

// obtain max speed in [m/s] from moved distance, last and current location
float getSpeed(Location location) {
    float dist = gps.distanceBetween(lastLoc.location.lat(), lastLoc.location.lng(), location.location.lat(), location.location.lng());
    time_t t_diff = (location.time.value() / 100) - (lastLoc.time.value() / 100);
    return max(max(dist / t_diff, (float)location.speed.mps()), (float)lastLoc.speed.mps());
}

bool smartBeaconCornerPeg(Location location) {
    int SB_TURN_TIME = config.sb_turn_time;
    int SB_TURN_MIN = config.sb_turn_min;
    float SB_TURN_SLOPE = config.sb_turn_slope * 1.0;

    float speed = location.speed.mps();
    time_t t_diff = (location.time.value() / 100) - (lastLoc.time.value() / 100);
    float turn = getBearingAngle(location.course.deg(), lastLoc.course.deg());

    // no bearing / stillstand -> no corner pegging
    if (!location.course.isValid() || speed == 0)
        return false;

    // if last bearing unknown, deploy turn_time
    if (!lastLoc.course.isValid())
        return (t_diff >= SB_TURN_TIME);

    // threshold depends on slope/speed [mph]
    float threshold = SB_TURN_MIN + SB_TURN_SLOPE / (speed * 2.23693629);

    log_i("%1.0f < %1.0f %ld/%d", turn, threshold, t_diff, SB_TURN_TIME);
    // need to corner peg if turn time reached and turn > threshold
    return (t_diff >= SB_TURN_TIME && turn > threshold);
}

// return true if current position is "new enough" vs. lastLoc
bool smartBeaconCheck(Location location) {
    if (!lastLoc.valid)
        return true;
    if (smartBeaconCornerPeg(location))
        return true;
    float dist = gps.distanceBetween(location.location.lat(), location.location.lng(), lastLoc.location.lat(), lastLoc.location.lng());
    time_t t_diff = (location.time.value() / 100) - (lastLoc.time.value() / 100);
    // log_d("location.time.value(): %ld", location.time.value());     // /100 => in seconds
    float speed = location.speed.mps();
    //if (location.speed.isValid() && location.course.isValid())
    int speed_rate = smartBeaconSpeedRate(speed);
    log_i("%1.0fm, %1.2fm/s -> %ld/%ds - %s", dist, speed, t_diff, speed_rate, (t_diff >= speed_rate) ? "true" : "false");
    if (t_diff >= speed_rate) {
        // log_i("%1.0fm, %1.2fm/s -> %ld/%ds - %s", dist, speed, t_diff, speed_rate, (t_diff >= speed_rate) ? "true" : "false");
        return true;
    } else {
        return false;
    }
}

bool SmartBeaconingProc() {
    Location location = {
        .valid = true,
        .location = gps.location,
        .course = gps.course,
        .speed = gps.speed,
        .time = gps.time
    };

    if (config.aprs_beacon > 0) {   // it interval configured
        return false;
    }

    if (smartBeaconCheck(location)) {
        lastLoc = location;
        return true;
    }

    return false;
}

// bool SmartBeaconingProc() {
// #ifdef USE_SMART_BEACONING

//     // https://github.com/ge0rg/aprsdroid/blob/master/src/location/SmartBeaconing.scala

//     float speed = gps.speed.kmph();
//     float course_prev = gps.course.deg();
//     float heading_change_since_beacon = abs(course_prev - gps.course.deg());
//     long beacon_rate, secs_since_beacon, turn_time;

//     if (speed < APRS_SB_LOW_SPEED) {
//         beacon_rate = config.sb_slow_rate;
//     } else {
//         // Adjust beacon rate according to speed
//         if (speed > APRS_SB_HIGH_SPEED) {
//             beacon_rate = config.sb_fast_rate;
//         } else {
//             beacon_rate = config.sb_fast_rate * APRS_SB_HIGH_SPEED / speed;
//         }

//         // Corner pegging - ALWAYS occurs if not "stopped"
//         // Note turn threshold is speed-dependent

//         // what is mph?
//         float turn_threshold = APRS_SB_TURN_MIN + config.sb_turn_slope / (speed * 2.23693629);

//         if ((heading_change_since_beacon > APRS_SB_TURN_TH) && (secs_since_beacon > turn_time)) {
//             secs_since_beacon = beacon_rate;
//         }
//     }

//     if (secs_since_beacon > beacon_rate) {
//         return true;
//     } else {
//         return false;
//     }
// #endif
// }
