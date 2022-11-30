/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the Web Service functionality.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include <Update.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <TimeLib.h>

#include "main.h"
#include "config.h"
#include "pkgList.h"

extern statusType status;
extern digiTLMType digiTLM;
extern Configuration config;
extern TaskHandle_t taskNetworkHandle;
extern TaskHandle_t taskAPRSHandle;
extern time_t systemUptime;
extern pkgListType pkgList[PKGLISTSIZE];

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

void serviceHandle();
void setHTML(byte page);
void handle_root();
void handle_setting();
void handle_service();
void handle_system();
void handle_firmware();
void handle_default();
#ifdef SDCRAD
void handle_storage();
void handle_download();
void handle_delete();
void listDir(fs::FS& fs, const char* dirname, uint8_t levels);
#endif
void webService();
#ifdef USE_RF
void handle_radio();
extern bool RF_Init(bool boot);
#endif
