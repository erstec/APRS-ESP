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

#include "sb.h"
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

// Convert TinyGPS++ time.value() (HHMMSSCC, i.e. HHMMSS * 100 + centiseconds) to seconds since midnight
static int32_t timeToSecs(uint32_t v) {
    return (int32_t)(v / 1000000) * 3600
         + (int32_t)((v / 10000) % 100) * 60
         + (int32_t)((v / 100) % 100);
}

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
    float delta = fmod(fabsf(alpha - beta), 360.0f);
    if (delta <= 180) 
        return delta;
    else 
        return (360 - delta);
}

bool sbCorner(Location location) {
    int SB_TURN_TIME = config.sb_turn_time;
    int SB_TURN_MIN = config.sb_turn_min;
    float SB_TURN_SLOPE = config.sb_turn_slope * 1.0;

    float speed = location.speed.mps();
    int32_t t_diff = timeToSecs(location.time.value()) - timeToSecs(lastLoc.time.value());
    if (t_diff < 0) t_diff += 86400;
    float turn = getBearingAngle(location.course.deg(), lastLoc.course.deg());

    if (!location.course.isValid() || speed == 0) {
        log_i("[SB] {\"fn\":\"corner\",\"result\":\"skip\",\"reason\":\"no_course_or_speed\",\"elapsed_s\":%ld}", t_diff);
        return false;
    }

    if (!lastLoc.course.isValid()) {
        bool peg = (t_diff >= SB_TURN_TIME);
        log_i("[SB] {\"fn\":\"corner\",\"result\":\"%s\",\"reason\":\"no_last_course\",\"elapsed_s\":%ld,\"turn_time_s\":%d}", peg ? "peg" : "wait", t_diff, SB_TURN_TIME);
        return peg;
    }

    float threshold = SB_TURN_MIN + SB_TURN_SLOPE / (speed * 2.23693629);
    bool peg = (t_diff >= SB_TURN_TIME && turn > threshold);
    log_i("[SB] {\"fn\":\"corner\",\"result\":\"%s\",\"turn_deg\":%d,\"thr_deg\":%d,\"elapsed_s\":%ld,\"turn_time_s\":%d}", peg ? "peg" : "wait", (int)turn, (int)threshold, t_diff, SB_TURN_TIME);
    return peg;
}

// return true if current position is "new enough" vs. lastLoc
bool sbCheck(Location location) {
    if (!lastLoc.valid)
        return true;
    if (sbCorner(location))
        return true;
    float dist = gps.distanceBetween(location.location.lat(), location.location.lng(), lastLoc.location.lat(), lastLoc.location.lng());
    int32_t t_diff = timeToSecs(location.time.value()) - timeToSecs(lastLoc.time.value());
    if (t_diff < 0) t_diff += 86400;
    float speed = location.speed.mps();
    //if (location.speed.isValid() && location.course.isValid())
    int speed_rate = smartBeaconSpeedRate(speed);
    log_i("[SB] {\"fn\":\"check\",\"dist_m\":%d,\"spd_kmh\":%d,\"elapsed_s\":%ld,\"rate_s\":%d,\"will_tx\":%d}", (int)dist, (int)(speed*3.6f), t_diff, speed_rate, (t_diff >= speed_rate) ? 1 : 0);
    if (t_diff >= speed_rate) {
        // log_i("%1.0fm, %1.2fm/s -> %ld/%ds - %s", dist, speed, t_diff, speed_rate, (t_diff >= speed_rate) ? "true" : "false");
        return true;
    } else {
        return false;
    }
}

bool sbProc() {
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

    if (sbCheck(location)) {
        lastLoc = location;
        return true;
    }

    return false;
}
