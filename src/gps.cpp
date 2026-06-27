/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the GPS processing.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/sh123/aprs_tracker
*/

#include <WiFi.h>
#include "gps.h"
#include "oled.h"
#include "sb.h"
#include "TinyGPS++.h"
#include "config.h"
#include "TimeLib.h"

// #define DEBUG_GPS

// extern Configuration config;
extern TinyGPSPlus gps;
extern HardwareSerial SerialGPS;
extern Configuration config;

extern teTimeSync timeSyncFlag;

// buffer for conversions
#define CONV_BUF_SIZE 15
static char conv_buf[CONV_BUF_SIZE];

// static bool gotGpsFix = false;

#define USE_PRECISE_DISTANCE
#define GPS_POLL_DURATION_MS 1000  // for how long to poll gps

long lat = 0;
long lon = 0;
long lat_prev = 0;
long lon_prev = 0;
unsigned long distance = 0;
unsigned long age = 0;

bool send_aprs_update = false;

int gnssSatsGPS = 0;
int gnssSatsBDS = 0;
int gnssSatsGLO = 0;

// Parse any *GSA sentence and update per-constellation satellite-in-use counts.
// Handles NMEA 4.11 ($GNGSA with sysId field 18) and standard NMEA ($GPGSA,
// $GLGSA, $BDGSA without sysId — constellation inferred from TalkerID).
static void parseGSA(const char *nmea) {
    char buf[82];
    strncpy(buf, nmea, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *star = strchr(buf, '*');
    if (!star) return;
    *star = 0;

    int commas = 0;
    for (const char *c = buf; *c; c++) if (*c == ',') commas++;

    int sysId = 0;
    if (commas >= 18) {
        // NMEA 4.11: last field before * is sysId
        char *lastComma = strrchr(buf, ',');
        sysId = atoi(lastComma + 1);
        *lastComma = 0;
    } else if (commas == 17) {
        // Standard NMEA: derive constellation from TalkerID prefix
        if      (strncmp(buf, "$GP", 3) == 0) sysId = 1;  // GPS
        else if (strncmp(buf, "$GL", 3) == 0) sysId = 2;  // GLONASS
        else if (strncmp(buf, "$BD", 3) == 0 ||
                 strncmp(buf, "$GB", 3) == 0) sysId = 4;  // BeiDou
        else return;  // $GNGSA without sysId — cannot determine
    } else {
        return;
    }

    // count non-empty PRN slots in fields 3..14
    int field = 0, satCount = 0;
    bool prevComma = false;
    for (const char *p = buf; *p; p++) {
        if (*p == ',') {
            field++;
            prevComma = true;
        } else if (prevComma) {
            if (field >= 3 && field <= 14) satCount++;
            prevComma = false;
        }
    }

    switch (sysId) {
        case 1: gnssSatsGPS = satCount; break;
        case 2: gnssSatsGLO = satCount; break;
        case 4: gnssSatsBDS = satCount; break;
    }
}

void distanceChanged() {
    if (sbProc()) {
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
        distance += distanceBetween(lat_prev, lon_prev, lat, lon);
#endif
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

static uint32_t gpsPktCnt = 0;

static const uint16_t gpsValidThr = 60 * 100; // 60 sec
static uint16_t gpsOfflineCnt = gpsValidThr;

uint32_t GpsPktCnt() {
    return gpsPktCnt;
}

void GpsUpdate() {
#ifdef USE_GPS
    //     for (unsigned long start = millis(); millis() - start <
    //     GPS_POLL_DURATION_MS;)
    //     {
    if (gpsOfflineCnt++ >= gpsValidThr) {
        if (gpsOfflineCnt >= UINT16_MAX) gpsOfflineCnt = gpsValidThr;
        gpsPktCnt = 0;
    }

    static char gsaBuf[82];
    static int  gsaLen = 0;

    while (SerialGPS.available()) {
        char c = SerialGPS.read();

        if (c == '\r') {
            // ignore
        } else if (c == '\n') {
            gsaBuf[gsaLen] = 0;
            if (gsaLen > 10 && strstr(gsaBuf, "GSA,"))
                parseGSA(gsaBuf);
            gsaLen = 0;
        } else if (gsaLen < (int)sizeof(gsaBuf) - 1) {
            gsaBuf[gsaLen++] = c;
        } else {
            gsaLen = 0;
        }

        if (gps.encode(c)) {
            if (gpsPktCnt++ >= UINT32_MAX) gpsPktCnt = 1;
            gpsOfflineCnt = 0;

            lat = gps.location.lat() * 1000000;
            lon = gps.location.lng() * 1000000;
            age = gps.location.age();

            if (config.locator_popup > 0 && gps.location.isValid()) {
                static char prevLocator[7] = {0};
                char locCopy[7];
                char empty[] = "";
                strlcpy(locCopy, deg_to_qth(lat, lon), sizeof(locCopy));
                if (prevLocator[0] == '\0') {
                    strlcpy(prevLocator, locCopy, sizeof(prevLocator));
                } else if (strncmp(prevLocator, locCopy, 6) != 0) {
                    strlcpy(prevLocator, locCopy, sizeof(prevLocator));
                    OledPushMsg("Locator", locCopy, empty, config.locator_popup);
                }
            }

            if (config.synctime 
                && (gps.time.age() / 1000) < 10
                && timeSyncFlag == T_SYNC_NONE
                && WiFi.status() != WL_CONNECTED) {
                if (gps.time.isValid() && gps.date.isValid() && gps.date.year() > 2000
                    && !(gps.time.hour() == 0 && gps.time.minute() == 0 && gps.time.second() == 0)) {
                    log_d("GPS DateTime: %04d-%02d-%02d %02d:%02d:%02d UTC",
                          gps.date.year(), gps.date.month(), gps.date.day(),
                          gps.time.hour(), gps.time.minute(), gps.time.second());

                    // Build proper UTC epoch — GPS provides UTC time+date
                    struct tm t = {};
                    t.tm_year  = gps.date.year() - 1900;
                    t.tm_mon   = gps.date.month() - 1;
                    t.tm_mday  = gps.date.day();
                    t.tm_hour  = gps.time.hour();
                    t.tm_min   = gps.time.minute();
                    t.tm_sec   = gps.time.second();
                    t.tm_isdst = 0;
                    setenv("TZ", "UTC0", 1);
                    tzset();
                    timeval tv = { mktime(&t), 0 };
                    settimeofday(&tv, nullptr);
                    // Restore configured timezone (POSIX sign is inverted vs UTC offset)
                    char tzBuf[12];
                    snprintf(tzBuf, sizeof(tzBuf), "UTC%+d", -config.timeZone);
                    setenv("TZ", tzBuf, 1);
                    tzset();

                    // set internal rtc time
                    setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
                    
                    timeSyncFlag = T_SYNC_GPS;

                    TimeSyncPeriod = millis() + (60 * 60 * 1000); // 60 min
                }
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
    float uDD_DDDDD = abs(DD_DDDDD);  //получили модуль числа
    *DD = (int)uDD_DDDDD;  //сделали из 37.45545 это 37 т.е. Градусы
    *MM = (int)((uDD_DDDDD - *DD) * 60);         //получили минуты
    *SS = ((uDD_DDDDD - *DD) * 60 - *MM) * 100;  //получили секунды
}

/*
**  Convert degrees in long format to NMEA string format
**  DDMM.hhN for latitude and DDDMM.hhW for longitude
*/
char *deg_to_nmea(long deg, boolean is_lat) {
    unsigned long abs_deg = (deg < 0) ? (unsigned long)(-deg) : (unsigned long)deg;
    unsigned long b = (abs_deg % 1000000UL) * 60UL;
    unsigned long a = (abs_deg / 1000000UL) * 100UL + b / 1000000UL;
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
    // Separate buffer from conv_buf — deg_to_nmea and deg_to_qth are called from
    // different FreeRTOS cores; sharing conv_buf caused occasional garbled locator output.
    static char qth_buf[7];
    float lat_ = ((float)lat) / 1000000.0 + 90.0;
    float lon_ = ((float)lon) / 1000000.0 + 180.0;
    qth_buf[0] = 'A' + int(lon_ / 20.0);
    qth_buf[1] = 'A' + int(lat_ / 10.0);
    qth_buf[2] = '0' + int(fmod(lon_, 20) / 2.0);
    qth_buf[3] = '0' + int(fmod(lat_, 10) / 1.0);
    qth_buf[4] = 'A' + int((lon_ - (int(lon_ / 2.0) * 2)) / (5.0 / 60.0));
    qth_buf[5] = 'A' + int((lat_ - (int(lat_ / 1.0) * 1)) / (2.5 / 60.0));
    qth_buf[6] = '\0';
    return qth_buf;
}
