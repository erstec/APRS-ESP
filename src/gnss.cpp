#include "gnss.h"
#include "TinyGPSPlus.h"

// #define DEBUG_GPS

// extern Configuration config;
extern TinyGPSPlus gps;
extern HardwareSerial SerialGPS;

// static bool gotGpsFix = false;

#define USE_PRECISE_DISTANCE
#define GPS_POLL_DURATION_MS 1000 // for how long to poll gps

long lat = 0;
long lon = 0;
long lat_prev = 0;
long lon_prev = 0;
long distance = 0;
unsigned long age = 0;

long distanceBetween(long lat1, long long1, long lat2, long long2)
{
  return gps.distanceBetween(((float)lat1)/1000000.0, ((float)long1)/1000000.0, 
    ((float)lat2)/1000000.0, ((float)long2)/1000000.0);
}

void updateDistance()
{
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

void updateGpsData() {
#ifdef USE_GPS
//     for (unsigned long start = millis(); millis() - start < GPS_POLL_DURATION_MS;)
//     {
        while (SerialGPS.available())
        {
            char c = SerialGPS.read();
#ifdef DEBUG_GPS
            Serial.print(c); // Debug
#endif
            if (gps.encode(c))
            {
                lat = gps.location.lat() * 1000000;
                lon = gps.location.lng() * 1000000;
                age = gps.location.age();
                updateDistance();
                // if (!gotGpsFix && gps.location.isValid()) gotGpsFix = true;
            }

            // if (millis() - start >= GPS_POLL_DURATION_MS)
            //     break;
        }
        //yield();
//     }
#endif
}
