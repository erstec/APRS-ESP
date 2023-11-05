/*
    Name:       APRS-ESP is APRS Internet Gateway / Tracker / Digipeater
    Created:    2022-10-10
    Author:     Ernest / LY3PH
    License:    GNU General Public License v3.0
    Includes code from:
                https://github.com/nakhonthai/ESP32IGate
                https://github.com/sh123/aprs_tracker
*/

#ifndef MAIN_H
#define MAIN_H

/// Convert a preprocessor name into a quoted string
#define xstr(s) str(s)
#define str(s) #s

/// Convert a preprocessor name into a quoted string and if that string is empty use "unset"
#define optstr(s) (xstr(s)[0] ? xstr(s) : "unset")

// #define VERSION "1.10"
#define VERSION         xstr(APP_VERSION_SHORT)
#define VERSION_FULL    xstr(APP_VERSION)

#define DEBUG
#define DEBUG_IS
#define DEBUG_TNC
#define DEBUG_RF

//#define SDCARD
#define USE_GPS
//#define USE_BLE
//#define USE_ROTARY
#define USE_SMART_BEACONING

#if defined(USE_SCREEN_SSD1306) && defined(USE_SCREEN_SH1106)
#error "Only one screen can be specified at once in main.h"
#endif

#if defined(USE_SCREEN_SSD1306) || defined(USE_SCREEN_SH1106)
#define USE_SCREEN
#endif

#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
#define APRS_PREAMBLE	(300UL)
#define APRS_TAIL       (100UL)
#else
#define APRS_PREAMBLE	(500UL) // Stock2 Adjusted
#define APRS_TAIL       (100UL)
#endif

#define TNC_TELEMETRY_PERIOD    600000UL    // 10 minutes

#define USE_RF

// smart beaconing parameters
#define APRS_SB_FAST_SPEED      100         // At this speed or above, beacons will be transmitted at the Fast Rate
#define APRS_SB_FAST_RATE       60          // How often beacons will be sent when you are travelling at or above the Fast Speed
#define APRS_SB_SLOW_SPEED      5           // This is the speed below which you’re considered stationary
#define APRS_SB_SLOW_RATE       1200        // How often beacons will be sent when you are are below the Slow Speed
#define APRS_SB_MIN_TURN_TIME   10          // The smallest time interval between beacons when you are continuously changing direction
#define APRS_SB_MIN_TURN_ANGLE  15          // The minimum angle by which you must change course before it will trigger a beacon
#define APRS_SB_TURN_SLOPE      240         // This number, when divided by your current speed will be added to the Min Turn Angle in order to increase the turn threshold at lower speeds

#define EEPROM_SIZE 2048

#include "pinout.h"

#define WIFI_OFF_FIX 0
#define WIFI_AP_FIX 1
#define WIFI_STA_FIX 2
#define WIFI_AP_STA_FIX 3

#define IMPLEMENTATION FIFO

#define TZ 0      // (utc+) TZ in hours
#define DST_MN 0  // use 60mn for summer time in some countries
#define TZ_MN ((TZ)*60)
#define TZ_SEC ((TZ)*3600)
#define DST_SEC ((DST_MN)*60)

#define FORMAT_SPIFFS_IF_FAILED true

#ifdef BOARD_HAS_PSRAM
#define TLMLISTSIZE 10
#define PKGLISTSIZE 100
#define PKGTXSIZE 100
#else
#define TLMLISTSIZE 10
#define PKGLISTSIZE 10
#define PKGTXSIZE 10
#endif

#define FILTER_ALL 0				// Packet is disable all packet
#define FILTER_OBJECT (1 << 0)		// packet is an object
#define FILTER_ITEM (1 << 1)		// packet is an item
#define FILTER_MESSAGE (1 << 2)		// packet is a message
#define FILTER_WX (1 << 3)			// packet is WX data
#define FILTER_TELEMETRY (1 << 4)	// packet is telemetry
#define FILTER_QUERY (1 << 5)		// packet is a query
#define FILTER_STATUS (1 << 6)		// packet is status
#define FILTER_POSITION (1 << 7)	// packet is postion
#define FILTER_BUOY (1 << 8)		// packet is buoy
#define FILTER_MICE (1 << 9)		// packet is MIC-E
#define FILTER_THIRDPARTY (1 << 10) // packet is 3rd-party packet from INET2RF

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPIFFS.h>
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "soc/rtc_wdt.h"
#endif
#include <AX25.h>

#include "HardwareSerial.h"
#include "EEPROM.h"

enum M17Flags {
    DISCONNECTED = 1 << 0,
    CONNECTING = 1 << 1,
    M17_AUTH = 1 << 2,
    M17_CONF = 1 << 3,
    M17_OPTS = 1 << 4,
    CONNECTED_RW = 1 << 5,
    CONNECTED_RO = 1 << 6
};

typedef struct igateTLM_struct {
    uint16_t Sequence;
    unsigned long ParmTimeout;
    unsigned long TeleTimeout;
    uint8_t RF2INET;
    uint8_t INET2RF;
    uint8_t RX;
    uint8_t TX;
    uint8_t DROP;
} igateTLMType;

typedef struct statisticStruct {
	uint32_t allCount;
	uint32_t tncCount;
	uint32_t isCount;
	uint32_t locationCount;
	uint32_t wxCount;
	uint32_t digiCount;
	uint32_t errorCount;
	uint32_t dropCount;
	uint32_t rf2inet;
	uint32_t inet2rf;
	uint32_t txCount;
	uint32_t rxCount;
} statusType;

typedef struct digiTLM_struct {
    unsigned int Sequence;
    unsigned int ParmTimeout;
    unsigned int TeleTimeout;
    unsigned char RxPkts;
    unsigned char TxPkts;
    unsigned char DropRx;
    unsigned char ErPkts;
} digiTLMType;

typedef struct Telemetry_struct {
	time_t time;
	char callsign[10];
	char PARM[5][10];
	char UNIT[5][10];
	float VAL[5];
	float RAW[5];
	float EQNS[15];
	uint8_t BITS;
	uint8_t BITS_FLAG;
	bool EQNS_FLAG;
} TelemetryType;

typedef struct txQueue_struct {
    bool Active;
    long timeStamp;
    int Delay;
    char Info[500];
} txQueueType;

extern bool fwUpdateProcess;

const char PARM[] = {"PARM.RF->INET,INET->RF,DigiRpt,TX2RF,DropRx"};
const char UNIT[] = {"UNIT.Pkts,Pkts,Pkts,Pkts,Pkts"};
const char EQNS[] = {"EQNS.0,1,0,0,1,0,0,1,0,0,1,0,0,1,0"};

const float ctcss[] = {0,     67,    71.9,  74.4,  77,    79.7,  82.5,  85.4,
                       88.5,  91.5,  94.8,  97.4,  100,   103.5, 107.2, 110.9,
                       114.8, 118.8, 123,   127.3, 131.8, 136.5, 141.3, 146.2,
                       151.4, 156.7, 162.2, 167.9, 173.8, 179.9, 186.2, 192.8,
                       203.5, 210.7, 218.1, 225.7, 233.6, 241.8, 250.3};
const float wifiPwr[12][2] = {{-4, -1},  {8, 2},     {20, 5},  {28, 7},
                              {34, 8.5}, {44, 11},   {52, 13}, {60, 15},
                              {68, 17},  {74, 18.5}, {76, 19}, {78, 19.5}};
const String gpsMode[3] = {"Auto", "GPS Only", "Fixed Only"};

enum gpsMode_enum { 
    GPS_MODE_AUTO = 0,
    GPS_MODE_GPS = 1,
    GPS_MODE_FIXED = 2
};

typedef enum {
    T_SYNC_NONE = 0,
    T_SYNC_NTP,
    T_SYNC_GPS,
    T_SYNC_APRS,
} teTimeSync;

extern teTimeSync timeSyncFlag;
extern long TimeSyncPeriod;

extern bool psramBusy;

void taskAPRS(void *pvParameters);
void taskNetwork(void *pvParameters);
void taskOLEDDisplay(void *pvParameters);
int processPacket(String &tnc2);
String send_gps_location();
int digiProcess(AX25Msg &Packet);
bool pkgTxPush(const char *info, size_t len, int dly);

#endif
