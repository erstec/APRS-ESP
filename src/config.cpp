/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with Configuration, stored in EEPROM.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include <ArduinoJson.h>
#include "config.h"

extern Configuration config;
extern bool input_HPF;

uint8_t checkSum(uint8_t *ptr, size_t count) {
    uint8_t lrc, tmp;
    uint16_t i;
    lrc = 0;
    for (i = 0; i < count; i++) {
        tmp = *ptr++;
        lrc = lrc ^ tmp;
    }
    return lrc;
}

void SaveConfig(bool storeBackup) {
    uint8_t chkSum = 0;
    byte *ptr;
    ptr = (byte *)&config;
    Serial.println("Saving config to EEPROM...");
    EEPROM.writeBytes(1, ptr, sizeof(Configuration));
    chkSum = checkSum(ptr, sizeof(Configuration));
    EEPROM.write(0, chkSum);
    EEPROM.commit();
#ifdef DEBUG
    Serial.print("Save EEPROM ChkSUM=");
    Serial.println(chkSum, HEX);
#endif
    
    if (!storeBackup) return;

    Serial.println("Saving config Backup to SPIFFS...");
    SPIFFS.begin(true);

    // '.bin' file
    File f = SPIFFS.open("/config.bin", "w");
    if (!f) {
        Serial.println("Failed to open config file for writing");
        SPIFFS.end();
        return;
    }
    f.write(chkSum);
    f.write((byte *)&config, sizeof(Configuration));
    f.close();

    // '.json' file
    File f_json = SPIFFS.open("/config.json", "w");
    if (!f_json) {
        Serial.println("Failed to open config file for writing");
        SPIFFS.end();
        return;
    }

    DynamicJsonDocument doc(2048);

    doc["synctime"] = config.synctime;
    doc["aprs"] = config.aprs;
    doc["wifi_client"] = config.wifi_client;
    doc["wifi"] = config.wifi;
    doc["wifi_mode"] = config.wifi_mode;
    doc["gps_lat"] = config.gps_lat;
    doc["gps_lon"] = config.gps_lon;
    doc["gps_alt"] = config.gps_alt;
    doc["aprs_ssid"] = config.aprs_ssid;
    doc["aprs_port"] = config.aprs_port;
    doc["aprs_moniSSID"] = config.aprs_moniSSID;
    doc["api_id"] = config.api_id;
    doc["tnc"] = config.tnc;
    doc["rf2inet"] = config.rf2inet;
    doc["inet2rf"] = config.inet2rf;
    doc["tnc_digi"] = config.tnc_digi;
    doc["tnc_telemetry"] = config.tnc_telemetry;
    doc["tnc_beacon"] = config.tnc_beacon;
    doc["aprs_beacon"] = config.aprs_beacon;
    doc["aprs_table"] = (uint8_t)config.aprs_table;
    doc["aprs_symbol"] = (uint8_t)config.aprs_symbol;
    doc["aprs_mycall"] = config.aprs_mycall;
    doc["aprs_host"] = config.aprs_host;
    doc["aprs_passcode"] = config.aprs_passcode;
    doc["aprs_moniCall"] = config.aprs_moniCall;
    doc["aprs_filter"] = config.aprs_filter;
    doc["aprs_comment"] = config.aprs_comment;
    doc["aprs_path"] = config.aprs_path;
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_pass"] = config.wifi_pass;
    doc["wifi_ap_ssid"] = config.wifi_ap_ssid;
    doc["wifi_ap_pass"] = config.wifi_ap_pass;
    doc["tnc_path"] = config.tnc_path;
    doc["tnc_btext"] = config.tnc_btext;
    doc["tnc_comment"] = config.tnc_comment;
    doc["aprs_object"] = config.aprs_object;
    doc["wifi_power"] = config.wifi_power;
    doc["tx_timeslot"] = config.tx_timeslot;
    doc["digi_delay"] = config.digi_delay;
    doc["input_hpf"] = config.input_hpf;
#ifdef USE_RF
    doc["freq_rx"] = config.freq_rx;
    doc["freq_tx"] = config.freq_tx;
    doc["offset_rx"] = config.offset_rx;
    doc["offset_tx"] = config.offset_tx;
    doc["tone_rx"] = config.tone_rx;
    doc["tone_tx"] = config.tone_tx;
    doc["band"] = config.band;
    doc["sql_level"] = config.sql_level;
    doc["rf_power"] = config.rf_power;
    doc["volume"] = config.volume;
#endif
    doc["timeZone"] = config.timeZone;
    doc["ntpServer"] = config.ntpServer;
    doc["gps_mode"] = config.gps_mode;

    serializeJsonPretty(doc, Serial);
    serializeJsonPretty(doc, f_json);
    f_json.close();
    
    SPIFFS.end();
}

void DefaultConfig() {
    Serial.println("Applying Factory Default configuration!");
    sprintf(config.aprs_mycall, "MYCALL");
    config.aprs_ssid = 15;
    sprintf(config.aprs_host, "rotate.aprs2.net");
    config.aprs_port = 14580;
    sprintf(config.aprs_passcode, "00000");
    sprintf(config.aprs_moniCall, "%s-%d", config.aprs_mycall, config.aprs_ssid);
    sprintf(config.aprs_filter, "m/150"); // g/LY*/SP*
    sprintf(config.wifi_ssid, "APRS-ESP32");
    sprintf(config.wifi_pass, "aprs-esp32");
    sprintf(config.wifi_ap_ssid, "APRS-ESP32");
    sprintf(config.wifi_ap_pass, "aprs-esp32");
    config.wifi_client = true;
    config.synctime = true;
    config.aprs_beacon = 10;    // minutes
    config.gps_lat = 54.6842;
    config.gps_lon = 25.2398;
    config.gps_alt = 10;
    config.tnc = false;
    config.inet2rf = false;
    config.rf2inet = false;
    config.aprs = false;
    config.wifi = true;
    config.wifi_mode = WIFI_AP_FIX;
    config.tnc_digi = false;
    config.tnc_telemetry = false;
    config.tnc_btext[0] = 0;
    config.tnc_beacon = 0;
    config.aprs_table = '/';
    config.aprs_symbol = '&';
    config.digi_delay = 2000;
    config.tx_timeslot = 5000;
    sprintf(config.aprs_path, "WIDE1-1");
    memset(config.aprs_object, 0, sizeof(config.aprs_object));
    sprintf(config.aprs_comment, "ESP IG github.com/erstec/APRS-ESP");
    sprintf(config.tnc_comment, "APRS-ESP Built in TNC");
    sprintf(config.aprs_filter, "m/150");
    sprintf(config.tnc_path, "WIDE1-1");
    config.wifi_power = 44;
    config.input_hpf = true;
#ifdef USE_RF
#ifndef BAND_70CM
    config.freq_rx = 144.8000;
    config.freq_tx = 144.8000;
#else
    config.freq_rx = 432.5000;
    config.freq_tx = 432.5000;
#endif /* BAND_70CM */
    config.offset_rx = 0;
    config.offset_tx = 0;
    config.tone_rx = 0;
    config.tone_tx = 0;
    config.band = 1;
    config.sql_level = 1;
    config.rf_power = LOW;
    config.volume = 4;
    config.input_hpf = false;
#endif
    input_HPF = config.input_hpf;
    config.timeZone = 0;
    sprintf(config.ntpServer, "pool.ntp.org");
    config.gps_mode = GPS_MODE_FIXED;    // 0 - Auto, 1 - GPS only, 2 - Fixed only
    SaveConfig();
}

void LoadConfig() {
    byte *ptr;

    Serial.println("Loading config from EEPROM...");

    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println(F("failed to initialise EEPROM"));  // delay(100000);
        ESP.restart();
    }

    delay(3000);
    
    uint8_t bootPin2 = HIGH;
#ifndef USE_ROTARY
    bootPin2 = digitalRead(PIN_ROT_BTN);
#endif

    if (digitalRead(BOOT_PIN) == LOW || bootPin2 == LOW) {
        DefaultConfig();
        Serial.println("Restoring Factory Default config");
        while (digitalRead(BOOT_PIN) == LOW || bootPin2 == LOW) {
#ifndef USE_ROTARY
            bootPin2 = digitalRead(PIN_ROT_BTN);
#endif
        };
    }

    // Check for configuration errors
    ptr = (byte *)&config;
    EEPROM.readBytes(1, ptr, sizeof(Configuration));
    uint8_t chkSum = checkSum(ptr, sizeof(Configuration));
    Serial.printf("EEPROM Check %0Xh=%0Xh(%dByte)\r\n", EEPROM.read(0), chkSum, sizeof(Configuration));

    // If EEPROM is corrupted, then restore from backup
    if (EEPROM.read(0) != chkSum) {
        // Restore from backup
        Serial.println("Config EEPROM CRC Error! Trying restore from backup...");
        if (!LoadReConfig()) {
            // If backup is corrupted, then restore factory defaults
            DefaultConfig();
            Serial.println("Config EEPROM CRC Error! Restoring Factory Default config");
        }
    } else {
        // If EEPORM is OK, then check Backup
        SPIFFS.begin(true);
        File f = SPIFFS.open("/config.bin", "r");
        if (!f) {
            Serial.println("Failed to open config file for reading");
            SPIFFS.end();
            esp_restart();
        }
    
        Configuration tmpConfig;

        uint8_t chkSum2 = f.read();
        size_t sz = f.read((byte *)&tmpConfig, sizeof(Configuration));
        f.close();
        SPIFFS.end();

        bool validSPIFFScfg = true;

        if (sz != sizeof(Configuration)) {
            validSPIFFScfg = false;
        }

        ptr = (byte *)&tmpConfig;

        uint8_t chkSum3 = checkSum(ptr, sizeof(Configuration));
        Serial.printf("SPIFFS Check %0Xh=%0Xh(%dByte)\r\n", chkSum2, chkSum3, sizeof(Configuration));

        if (chkSum2 != chkSum3) {
            validSPIFFScfg = false;
        }

        if (!validSPIFFScfg) {
            Serial.println("SPIFFS Config File Error! Reparing...");
            SPIFFS.begin(true);
            File f = SPIFFS.open("/config.bin", "w");
            if (!f) {
                Serial.println("Failed to open SPIFFS config file for writing");
                SPIFFS.end();
                esp_restart();
            }
            f.write(chkSum);
            f.write((byte *)&config, sizeof(Configuration));
            f.close();
            SPIFFS.end();
        }
    }
    input_HPF = config.input_hpf;

    // If JSON not here (transition from old version), then convert BIN to JSON and save to SPIFFS
    SPIFFS.begin(true);
    File f_json = SPIFFS.open("/config.json", "r");
    if (!f_json) {
        Serial.println("Config JSON file not found! Trying to convert BIN to JSON...");
        SPIFFS.end();
        LoadReConfig();
    } else {
        f_json.close();
        SPIFFS.end();
    }
}

Configuration jsonToBinConfig(JsonObject obj) {
    Configuration tmpConfig;

    tmpConfig.synctime = obj["synctime"];
    tmpConfig.aprs = obj["aprs"];
    tmpConfig.wifi_client = obj["wifi_client"];
    tmpConfig.wifi = obj["wifi"];
    tmpConfig.wifi_mode = obj["wifi_mode"];
    tmpConfig.gps_lat = obj["gps_lat"];
    tmpConfig.gps_lon = obj["gps_lon"];
    tmpConfig.gps_alt = obj["gps_alt"];
    tmpConfig.aprs_ssid = obj["aprs_ssid"];
    tmpConfig.aprs_port = obj["aprs_port"];
    tmpConfig.aprs_moniSSID = obj["aprs_moniSSID"];
    tmpConfig.api_id = obj["api_id"];
    tmpConfig.tnc = obj["tnc"];
    tmpConfig.rf2inet = obj["rf2inet"];
    tmpConfig.inet2rf = obj["inet2rf"];
    tmpConfig.tnc_digi = obj["tnc_digi"];
    tmpConfig.tnc_telemetry = obj["tnc_telemetry"];
    tmpConfig.tnc_beacon = obj["tnc_beacon"];
    tmpConfig.aprs_beacon = obj["aprs_beacon"];
    uint8_t tmp = obj["aprs_table"];
    tmpConfig.aprs_table = (char)tmp;
    tmp = obj["aprs_symbol"];
    tmpConfig.aprs_symbol = (char)tmp;
    strcpy(tmpConfig.aprs_mycall, obj["aprs_mycall"]);
    strcpy(tmpConfig.aprs_host, obj["aprs_host"]);
    strcpy(tmpConfig.aprs_passcode, obj["aprs_passcode"]);
    strcpy(tmpConfig.aprs_moniCall, obj["aprs_moniCall"]);
    strcpy(tmpConfig.aprs_filter, obj["aprs_filter"]);
    strcpy(tmpConfig.aprs_comment, obj["aprs_comment"]);
    strcpy(tmpConfig.aprs_path, obj["aprs_path"]);
    strcpy(tmpConfig.wifi_ssid, obj["wifi_ssid"]);
    strcpy(tmpConfig.wifi_pass, obj["wifi_pass"]);
    strcpy(tmpConfig.wifi_ap_ssid, obj["wifi_ap_ssid"]);
    strcpy(tmpConfig.wifi_ap_pass, obj["wifi_ap_pass"]);
    strcpy(tmpConfig.tnc_path, obj["tnc_path"]);
    strcpy(tmpConfig.tnc_btext, obj["tnc_btext"]);
    strcpy(tmpConfig.tnc_comment, obj["tnc_comment"]);
    strcpy(tmpConfig.aprs_object, obj["aprs_object"]);
    tmpConfig.wifi_power = obj["wifi_power"];
    tmpConfig.tx_timeslot = obj["tx_timeslot"];
    tmpConfig.digi_delay = obj["digi_delay"];
    tmpConfig.input_hpf = obj["input_hpf"];
#ifdef USE_RF
    tmpConfig.freq_rx = obj["freq_rx"];
    tmpConfig.freq_tx = obj["freq_tx"];
    tmpConfig.offset_rx = obj["offset_rx"];
    tmpConfig.offset_tx = obj["offset_tx"];
    tmpConfig.tone_rx = obj["tone_rx"];
    tmpConfig.tone_tx = obj["tone_tx"];
    tmpConfig.band = obj["band"];
    tmpConfig.sql_level = obj["sql_level"];
    tmpConfig.rf_power = obj["rf_power"];
    tmpConfig.volume = obj["volume"];
#endif
    tmpConfig.timeZone = obj["timeZone"];
    strcpy(tmpConfig.ntpServer, obj["ntpServer"]);
    tmpConfig.gps_mode = obj["gps_mode"];

    return tmpConfig;
}

bool LoadReConfig() {
    // JSON to BIN conversion
    Serial.println("Trying to load JSON config from SPIFFS...");
    SPIFFS.begin(true);
    File f_json = SPIFFS.open("/config.json", "r");
    if (!f_json) {
        Serial.println("Failed to open config file for reading");
        SPIFFS.end();
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, f_json);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        f_json.close();
        SPIFFS.end();
        return false;
    }
    f_json.close();
    SPIFFS.end();

    Configuration tmpConfig = jsonToBinConfig(doc.as<JsonObject>());

    // Save BIN config to SPIFFS
    Serial.println("Saving converted BIN config to SPIFFS...");
    SPIFFS.begin(true);
    File f = SPIFFS.open("/config.bin", "w");
    if (!f) {
        Serial.println("Failed to open config file for writing");
        SPIFFS.end();
        return false;
    }
    f.write(checkSum((byte *)&tmpConfig, sizeof(Configuration)));
    f.write((byte *)&tmpConfig, sizeof(Configuration));
    f.close();
    SPIFFS.end();

    memcpy(&config, &tmpConfig, sizeof(Configuration));
    input_HPF = config.input_hpf;
    SaveConfig(false);

    return true;
}
