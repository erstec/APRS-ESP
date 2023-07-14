/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with Configuration, stored in EEPROM.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "main.h"

typedef struct Config_Struct {
    bool synctime;
    bool aprs;
    bool wifi_client;
    bool wifi;
    char wifi_mode;  // WIFI_AP,WIFI_STA,WIFI_AP_STA,WIFI_OFF
    float gps_lat;
    float gps_lon;
    float gps_alt;
    uint8_t aprs_ssid;
    uint16_t aprs_port;
    uint8_t aprs_moniSSID;
    uint32_t api_id;
    bool tnc;
    bool rf2inet;
    bool inet2rf;
    bool tnc_digi = false;
    bool tnc_telemetry = false;
    int tnc_beacon = 0;
    int aprs_beacon;
    char aprs_table;
    char aprs_symbol;
    char aprs_mycall[10];
    char aprs_host[20];
    char aprs_passcode[10];
    char aprs_moniCall[10];
    char aprs_filter[30];
    char aprs_comment[50];
    char aprs_path[72];
    char wifi_ssid[32];
    char wifi_pass[63];
    char wifi_ap_ssid[32];
    char wifi_ap_pass[63];
    char tnc_path[50];
    char tnc_btext[50];
    char tnc_comment[50];
    char aprs_object[10];
    char wifi_power;
    uint16_t tx_timeslot;
    uint16_t digi_delay;
    bool input_hpf;
#ifdef USE_RF
    float freq_rx;
    float freq_tx;
    int offset_rx;
    int offset_tx;
    int tone_rx;
    int tone_tx;
    uint8_t band;
    uint8_t sql_level;
    bool rf_power;
    uint8_t volume;
#endif
    int8_t timeZone;
} Configuration;

void DefaultConfig();
uint8_t checkSum(uint8_t *ptr, size_t count);
void SaveConfig(bool storeBackup = true);
void LoadConfig();
void LoadReConfig();

#endif
