/*
    Description:    This file is part of the APRS-ESP project.
                    Configuration stored in ESP32 NVS (Preferences).
                    JSON on LittleFS is used for web export/import only.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

extern Configuration config;
extern bool input_HPF;

static Configuration jsonToBinConfig(JsonObject obj);

static void checkCallsignValid(void) {
    if (strcmp("MYCALL", config.aprs_mycall) == 0
        || strcmp("NOCALL", config.aprs_mycall) == 0
        || strcmp("N0CALL", config.aprs_mycall) == 0
        || strlen(config.aprs_mycall) < 3) {
        callsignValid = false;
    } else {
        callsignValid = true;
    }

    if (callsignValid) {
        log_i("Callsign is valid");
    } else {
        log_w("Callsign is invalid");
    }
}

void autoUpdateComment() {
    char modes[24] = "";
    bool first = true;
    bool isTracker = config.tnc && (config.gps_mode != GPS_MODE_FIXED);
    if (isTracker)       { strlcat(modes, first ? "Tracker" : "+Tracker", sizeof(modes)); first = false; }
    if (config.tnc_digi) { strlcat(modes, first ? "Digi"    : "+Digi",    sizeof(modes)); first = false; }
    if (config.aprs)     { strlcat(modes, first ? "iGate"   : "+iGate",   sizeof(modes)); first = false; }

    char generated[50];
    if (modes[0])
        snprintf(generated, sizeof(generated), "ESP %s github.com/erstec/APRS-ESP", modes);
    else
        snprintf(generated, sizeof(generated), "ESP github.com/erstec/APRS-ESP");

    if (strncmp(config.aprs_comment, "ESP ", 4) == 0 && strstr(config.aprs_comment, "github.com/erstec/APRS-ESP") != NULL)
        strlcpy(config.aprs_comment, generated, sizeof(config.aprs_comment));
}

static void nvsWrite(const char *ns) {
    Preferences prefs;
    prefs.begin(ns, false);

    prefs.putBool  ("synctime",      config.synctime);
    prefs.putBool  ("aprs",          config.aprs);
    prefs.putBool  ("wifi_client",   config.wifi_client);
    prefs.putBool  ("wifi",          config.wifi);
    prefs.putUChar ("wifi_mode",     config.wifi_mode);
    prefs.putFloat ("gps_lat",       config.gps_lat);
    prefs.putFloat ("gps_lon",       config.gps_lon);
    prefs.putFloat ("gps_alt",       config.gps_alt);
    prefs.putUChar ("aprs_ssid",     config.aprs_ssid);
    prefs.putUShort("aprs_port",     config.aprs_port);
    prefs.putUChar ("aprs_moniSSID", config.aprs_moniSSID);
    prefs.putUInt  ("api_id",        config.api_id);
    prefs.putBool  ("tnc",           config.tnc);
    prefs.putBool  ("rf2inet",       config.rf2inet);
    prefs.putBool  ("inet2rf",       config.inet2rf);
    prefs.putBool  ("tnc_digi",      config.tnc_digi);
    prefs.putBool  ("tnc_telemetry", config.tnc_telemetry);
    prefs.putInt   ("tnc_beacon",    config.tnc_beacon);
    prefs.putInt   ("aprs_beacon",   config.aprs_beacon);
    prefs.putUChar ("aprs_table",    (uint8_t)config.aprs_table);
    prefs.putUChar ("aprs_symbol",   (uint8_t)config.aprs_symbol);
    prefs.putString("aprs_mycall",   config.aprs_mycall);
    prefs.putString("aprs_host",     config.aprs_host);
    prefs.putString("aprs_passcode", config.aprs_passcode);
    prefs.putString("aprs_moniCall", config.aprs_moniCall);
    prefs.putString("aprs_filter",   config.aprs_filter);
    prefs.putString("aprs_comment",  config.aprs_comment);
    prefs.putString("aprs_path",     config.aprs_path);
    prefs.putString("wifi_ssid",     config.wifi_ssid);
    prefs.putString("wifi_pass",     config.wifi_pass);
    prefs.putString("wifi_ap_ssid",  config.wifi_ap_ssid);
    prefs.putString("wifi_ap_pass",  config.wifi_ap_pass);
    prefs.putString("tnc_path",      config.tnc_path);
    prefs.putString("tnc_btext",     config.tnc_btext);
    prefs.putString("tnc_comment",   config.tnc_comment);
    prefs.putString("aprs_object",   config.aprs_object);
    prefs.putUChar ("wifi_power",    config.wifi_power);
    prefs.putUShort("tx_timeslot",   config.tx_timeslot);
    prefs.putUShort("digi_delay",    config.digi_delay);
    prefs.putBool  ("input_hpf",     config.input_hpf);
    prefs.putFloat ("freq_rx",       config.freq_rx);
    prefs.putFloat ("freq_tx",       config.freq_tx);
    prefs.putInt   ("offset_rx",     config.offset_rx);
    prefs.putInt   ("offset_tx",     config.offset_tx);
    prefs.putInt   ("tone_rx",       config.tone_rx);
    prefs.putInt   ("tone_tx",       config.tone_tx);
    prefs.putUChar ("band",          config.band);
    prefs.putUChar ("sql_level",     config.sql_level);
    prefs.putBool  ("rf_power",      config.rf_power);
    prefs.putUChar ("volume",        config.volume);
    prefs.putBool  ("rx_att",        config.rx_att);
    prefs.putChar  ("timeZone",      config.timeZone);
    prefs.putString("ntpServer",     config.ntpServer);
    prefs.putUChar ("gps_mode",      config.gps_mode);
    prefs.putUShort("sb_fast_speed", config.sb_fast_speed);
    prefs.putUShort("sb_fast_rate",  config.sb_fast_rate);
    prefs.putUShort("sb_slow_speed", config.sb_slow_speed);
    prefs.putUShort("sb_slow_rate",  config.sb_slow_rate);
    prefs.putUShort("sb_turn_min",   config.sb_turn_min);
    prefs.putUShort("sb_turn_slope", config.sb_turn_slope);
    prefs.putUShort("sb_turn_time",  config.sb_turn_time);
    prefs.putUChar ("oled_brightness",config.oled_brightness);
    prefs.putUChar ("locator_popup", config.locator_popup);
    prefs.putUChar ("oled_autodim",  config.oled_autodim);
    prefs.putUChar ("autodim_level", config.oled_autodim_level);  // key shortened: 15-char NVS limit
    prefs.putUChar ("aprs_rx_popup",  config.aprs_rx_popup);
    prefs.putUChar ("aprs_sms_popup", config.aprs_sms_popup);
    prefs.putBool  ("aprs_sms_rx",    config.aprs_sms_rx);

    prefs.end();
}

static bool nvsRead(const char *ns) {
    Preferences prefs;
    prefs.begin(ns, true);
    bool found = prefs.isKey("aprs_mycall");
    if (!found) {
        prefs.end();
        return false;
    }

    config.synctime        = prefs.getBool  ("synctime",      true);
    config.aprs            = prefs.getBool  ("aprs",          false);
    config.wifi_client     = prefs.getBool  ("wifi_client",   true);
    config.wifi            = prefs.getBool  ("wifi",          true);
    config.wifi_mode       = prefs.getUChar ("wifi_mode",     WIFI_AP_FIX);
    config.gps_lat         = prefs.getFloat ("gps_lat",       54.6842f);
    config.gps_lon         = prefs.getFloat ("gps_lon",       25.2398f);
    config.gps_alt         = prefs.getFloat ("gps_alt",       10.0f);
    config.aprs_ssid       = prefs.getUChar ("aprs_ssid",     15);
    config.aprs_port       = prefs.getUShort("aprs_port",     14580);
    config.aprs_moniSSID   = prefs.getUChar ("aprs_moniSSID", 0);
    config.api_id          = prefs.getUInt  ("api_id",        0);
    config.tnc             = prefs.getBool  ("tnc",           false);
    config.rf2inet         = prefs.getBool  ("rf2inet",       false);
    config.inet2rf         = prefs.getBool  ("inet2rf",       false);
    config.tnc_digi        = prefs.getBool  ("tnc_digi",      false);
    config.tnc_telemetry   = prefs.getBool  ("tnc_telemetry", false);
    config.tnc_beacon      = prefs.getInt   ("tnc_beacon",    0);
    config.aprs_beacon     = prefs.getInt   ("aprs_beacon",   10);
    config.aprs_table      = (char)prefs.getUChar("aprs_table",  '/');
    config.aprs_symbol     = (char)prefs.getUChar("aprs_symbol", '&');
    strlcpy(config.aprs_mycall,   prefs.getString("aprs_mycall",   "MYCALL").c_str(),           sizeof(config.aprs_mycall));
    strlcpy(config.aprs_host,     prefs.getString("aprs_host",     "rotate.aprs2.net").c_str(), sizeof(config.aprs_host));
    strlcpy(config.aprs_passcode, prefs.getString("aprs_passcode", "00000").c_str(),            sizeof(config.aprs_passcode));
    strlcpy(config.aprs_moniCall, prefs.getString("aprs_moniCall", "").c_str(),                 sizeof(config.aprs_moniCall));
    strlcpy(config.aprs_filter,   prefs.getString("aprs_filter",   "m/50").c_str(),             sizeof(config.aprs_filter));
    strlcpy(config.aprs_comment,  prefs.getString("aprs_comment",  "").c_str(),                 sizeof(config.aprs_comment));
    strlcpy(config.aprs_path,     prefs.getString("aprs_path",     "WIDE1-1").c_str(),          sizeof(config.aprs_path));
    strlcpy(config.wifi_ssid,     prefs.getString("wifi_ssid",     "APRS-ESP32").c_str(),       sizeof(config.wifi_ssid));
    strlcpy(config.wifi_pass,     prefs.getString("wifi_pass",     "aprs-esp32").c_str(),       sizeof(config.wifi_pass));
    strlcpy(config.wifi_ap_ssid,  prefs.getString("wifi_ap_ssid",  "APRS-ESP32").c_str(),       sizeof(config.wifi_ap_ssid));
    strlcpy(config.wifi_ap_pass,  prefs.getString("wifi_ap_pass",  "aprs-esp32").c_str(),       sizeof(config.wifi_ap_pass));
    strlcpy(config.tnc_path,      prefs.getString("tnc_path",      "WIDE1-1").c_str(),          sizeof(config.tnc_path));
    strlcpy(config.tnc_btext,     prefs.getString("tnc_btext",     "").c_str(),                 sizeof(config.tnc_btext));
    strlcpy(config.tnc_comment,   prefs.getString("tnc_comment",   "").c_str(),                 sizeof(config.tnc_comment));
    strlcpy(config.aprs_object,   prefs.getString("aprs_object",   "").c_str(),                 sizeof(config.aprs_object));
    config.wifi_power      = prefs.getUChar ("wifi_power",    44);
    config.tx_timeslot     = prefs.getUShort("tx_timeslot",   5000);
    config.digi_delay      = prefs.getUShort("digi_delay",    2000);
    config.input_hpf       = prefs.getBool  ("input_hpf",     false);
    config.freq_rx         = prefs.getFloat ("freq_rx",       144.8000f);
    config.freq_tx         = prefs.getFloat ("freq_tx",       144.8000f);
    config.offset_rx       = prefs.getInt   ("offset_rx",     0);
    config.offset_tx       = prefs.getInt   ("offset_tx",     0);
    config.tone_rx         = prefs.getInt   ("tone_rx",       0);
    config.tone_tx         = prefs.getInt   ("tone_tx",       0);
    config.band            = prefs.getUChar ("band",          1);
    config.sql_level       = prefs.getUChar ("sql_level",     1);
    config.rf_power        = prefs.getBool  ("rf_power",      false);
    config.volume          = prefs.getUChar ("volume",        4);
    config.rx_att          = prefs.getBool  ("rx_att",        false);
    config.timeZone        = prefs.getChar  ("timeZone",      0);
    strlcpy(config.ntpServer, prefs.getString("ntpServer", "pool.ntp.org").c_str(), sizeof(config.ntpServer));
    config.gps_mode        = prefs.getUChar ("gps_mode",      GPS_MODE_FIXED);
    config.sb_fast_speed   = prefs.getUShort("sb_fast_speed", APRS_SB_FAST_SPEED);
    config.sb_fast_rate    = prefs.getUShort("sb_fast_rate",  APRS_SB_FAST_RATE);
    config.sb_slow_speed   = prefs.getUShort("sb_slow_speed", APRS_SB_SLOW_SPEED);
    config.sb_slow_rate    = prefs.getUShort("sb_slow_rate",  APRS_SB_SLOW_RATE);
    config.sb_turn_min     = prefs.getUShort("sb_turn_min",   APRS_SB_MIN_TURN_ANGLE);
    config.sb_turn_slope   = prefs.getUShort("sb_turn_slope", APRS_SB_TURN_SLOPE);
    config.sb_turn_time    = prefs.getUShort("sb_turn_time",  APRS_SB_MIN_TURN_TIME);
    config.oled_brightness    = prefs.getUChar ("oled_brightness", 255);
    config.locator_popup      = prefs.getUChar ("locator_popup", 0);
    config.oled_autodim       = prefs.getUChar ("oled_autodim",  0);
    config.oled_autodim_level = prefs.getUChar ("autodim_level", 1);
    config.aprs_rx_popup      = prefs.getUChar ("aprs_rx_popup",  3);
    config.aprs_sms_popup     = prefs.getUChar ("aprs_sms_popup", 60);
    config.aprs_sms_rx        = prefs.getBool  ("aprs_sms_rx",    true);

    prefs.end();
    return true;
}

void SaveConfig() {
    log_i("Saving config to NVS...");
    autoUpdateComment();
    nvsWrite("config");

    // Write JSON to LittleFS for web export/import
    File f_json = LittleFS.open("/config.json", "w");
    if (!f_json) {
        log_e("Failed to open config.json for writing");
        checkCallsignValid();
        return;
    }

    JsonDocument doc;
    doc["synctime"]          = config.synctime;
    doc["aprs"]              = config.aprs;
    doc["wifi_client"]       = config.wifi_client;
    doc["wifi"]              = config.wifi;
    doc["wifi_mode"]         = config.wifi_mode;
    doc["gps_lat"]           = config.gps_lat;
    doc["gps_lon"]           = config.gps_lon;
    doc["gps_alt"]           = config.gps_alt;
    doc["aprs_ssid"]         = config.aprs_ssid;
    doc["aprs_port"]         = config.aprs_port;
    doc["aprs_moniSSID"]     = config.aprs_moniSSID;
    doc["api_id"]            = config.api_id;
    doc["tnc"]               = config.tnc;
    doc["rf2inet"]           = config.rf2inet;
    doc["inet2rf"]           = config.inet2rf;
    doc["tnc_digi"]          = config.tnc_digi;
    doc["tnc_telemetry"]     = config.tnc_telemetry;
    doc["tnc_beacon"]        = config.tnc_beacon;
    doc["aprs_beacon"]       = config.aprs_beacon;
    doc["aprs_table"]        = (uint8_t)config.aprs_table;
    doc["aprs_symbol"]       = (uint8_t)config.aprs_symbol;
    doc["aprs_mycall"]       = config.aprs_mycall;
    doc["aprs_host"]         = config.aprs_host;
    doc["aprs_passcode"]     = config.aprs_passcode;
    doc["aprs_moniCall"]     = config.aprs_moniCall;
    doc["aprs_filter"]       = config.aprs_filter;
    doc["aprs_comment"]      = config.aprs_comment;
    doc["aprs_path"]         = config.aprs_path;
    doc["wifi_ssid"]         = config.wifi_ssid;
    doc["wifi_pass"]         = config.wifi_pass;
    doc["wifi_ap_ssid"]      = config.wifi_ap_ssid;
    doc["wifi_ap_pass"]      = config.wifi_ap_pass;
    doc["tnc_path"]          = config.tnc_path;
    doc["tnc_btext"]         = config.tnc_btext;
    doc["tnc_comment"]       = config.tnc_comment;
    doc["aprs_object"]       = config.aprs_object;
    doc["wifi_power"]        = config.wifi_power;
    doc["tx_timeslot"]       = config.tx_timeslot;
    doc["digi_delay"]        = config.digi_delay;
    doc["input_hpf"]         = config.input_hpf;
    doc["freq_rx"]           = config.freq_rx;
    doc["freq_tx"]           = config.freq_tx;
    doc["offset_rx"]         = config.offset_rx;
    doc["offset_tx"]         = config.offset_tx;
    doc["tone_rx"]           = config.tone_rx;
    doc["tone_tx"]           = config.tone_tx;
    doc["band"]              = config.band;
    doc["sql_level"]         = config.sql_level;
    doc["rf_power"]          = config.rf_power;
    doc["volume"]            = config.volume;
    doc["oled_brightness"]   = config.oled_brightness;
    doc["rx_att"]            = config.rx_att;
    doc["timeZone"]          = config.timeZone;
    doc["ntpServer"]         = config.ntpServer;
    doc["gps_mode"]          = config.gps_mode;
    doc["sb_fast_speed"]     = config.sb_fast_speed;
    doc["sb_fast_rate"]      = config.sb_fast_rate;
    doc["sb_slow_speed"]     = config.sb_slow_speed;
    doc["sb_slow_rate"]      = config.sb_slow_rate;
    doc["sb_turn_min"]       = config.sb_turn_min;
    doc["sb_turn_slope"]     = config.sb_turn_slope;
    doc["sb_turn_time"]      = config.sb_turn_time;
    doc["locator_popup"]     = config.locator_popup;
    doc["oled_autodim"]      = config.oled_autodim;
    doc["oled_autodim_level"]= config.oled_autodim_level;
    doc["aprs_rx_popup"]     = config.aprs_rx_popup;
    doc["aprs_sms_popup"]    = config.aprs_sms_popup;
    doc["aprs_sms_rx"]       = config.aprs_sms_rx;

    serializeJsonPretty(doc, f_json);
    f_json.close();

    checkCallsignValid();
}

void SaveConfigBackup() {
    log_i("Saving config backup to NVS cfgbak...");
    nvsWrite("cfgbak");
}

void DefaultConfig() {
    log_i("Applying Factory Default configuration!");
    sprintf(config.aprs_mycall, "MYCALL");
    config.aprs_ssid = 15;
    sprintf(config.aprs_host, "rotate.aprs2.net");
    config.aprs_port = 14580;
    sprintf(config.aprs_passcode, "00000");
    sprintf(config.aprs_moniCall, "%s-%d", config.aprs_mycall, config.aprs_ssid);
    sprintf(config.aprs_filter, "m/50");
    sprintf(config.wifi_ssid, "APRS-ESP32");
    sprintf(config.wifi_pass, "aprs-esp32");
    sprintf(config.wifi_ap_ssid, "APRS-ESP32");
    sprintf(config.wifi_ap_pass, "aprs-esp32");
    config.wifi_client = true;
    config.synctime = true;
    config.aprs_beacon = 10;
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
    sprintf(config.aprs_comment, "ESP github.com/erstec/APRS-ESP");
    sprintf(config.tnc_comment, "ESP github.com/erstec/APRS-ESP");
    sprintf(config.tnc_path, "WIDE1-1");
    config.wifi_power = 44;
    config.input_hpf = true;
#ifndef BAND_70CM
    config.freq_rx = 144.8000;
    config.freq_tx = 144.8000;
#else
    config.freq_rx = 432.5000;
    config.freq_tx = 432.5000;
#endif
    config.offset_rx = 0;
    config.offset_tx = 0;
    config.tone_rx = 0;
    config.tone_tx = 0;
    config.band = 1;
    config.sql_level = 1;
    config.rf_power = LOW;
    config.volume = 4;
    config.oled_brightness = 255;
    config.input_hpf = false;
    config.rx_att = false;
    input_HPF = config.input_hpf;
    config.timeZone = 0;
    sprintf(config.ntpServer, "pool.ntp.org");
    config.gps_mode = GPS_MODE_FIXED;

    config.sb_fast_speed = APRS_SB_FAST_SPEED;
    config.sb_fast_rate = APRS_SB_FAST_RATE;
    config.sb_slow_speed = APRS_SB_SLOW_SPEED;
    config.sb_slow_rate = APRS_SB_SLOW_RATE;
    config.sb_turn_min = APRS_SB_MIN_TURN_ANGLE;
    config.sb_turn_slope = APRS_SB_TURN_SLOPE;
    config.sb_turn_time = APRS_SB_MIN_TURN_TIME;
    config.locator_popup = 0;
    config.oled_autodim = 0;
    config.oled_autodim_level = 1;
    config.aprs_rx_popup  = 3;
    config.aprs_sms_popup = 60;
    config.aprs_sms_rx    = true;

    SaveConfig();
}

void LoadConfig() {
    delay(3000);

    uint8_t bootPin2 = HIGH;
#ifndef USE_ROTARY
    bootPin2 = digitalRead(PIN_ROT_BTN);
#endif

    if (digitalRead(BOOT_PIN) == LOW || bootPin2 == LOW) {
        log_i("Boot pin held — restoring factory defaults");
        DefaultConfig();
        while (digitalRead(BOOT_PIN) == LOW || bootPin2 == LOW) {
#ifndef USE_ROTARY
            bootPin2 = digitalRead(PIN_ROT_BTN);
#endif
        }
        return;
    }

    if (nvsRead("config")) {
        log_i("Config loaded from NVS");
        input_HPF = config.input_hpf;
        checkCallsignValid();
        return;
    }

    log_w("Primary NVS empty — trying backup NVS (cfgbak)...");
    if (nvsRead("cfgbak")) {
        log_w("Config restored from NVS backup — re-saving to primary");
        nvsWrite("config");
        input_HPF = config.input_hpf;
        checkCallsignValid();
        return;
    }

    // Both NVS namespaces empty — try JSON, otherwise factory defaults
    if (!LoadReConfig()) {
        log_e("No config found. Applying factory defaults.");
        DefaultConfig();
    }
}

bool LoadReConfig() {
    log_i("Loading config from JSON...");
    File f_json = LittleFS.open("/config.json", "r");
    if (!f_json) {
        log_e("config.json not found");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f_json);
    f_json.close();
    if (error) {
        log_e("deserializeJson() failed: %s", error.c_str());
        return false;
    }

    config = jsonToBinConfig(doc.as<JsonObject>());
    input_HPF = config.input_hpf;
    SaveConfig();  // persist to NVS + re-write JSON
    return true;
}

Configuration jsonToBinConfig(JsonObject obj) {
    Configuration tmpConfig = {};

    tmpConfig.synctime        = obj["synctime"]        | true;
    tmpConfig.aprs            = obj["aprs"]            | false;
    tmpConfig.wifi_client     = obj["wifi_client"]     | true;
    tmpConfig.wifi            = obj["wifi"]            | true;
    tmpConfig.wifi_mode       = obj["wifi_mode"]       | (uint8_t)WIFI_AP_FIX;
    tmpConfig.gps_lat         = obj["gps_lat"]         | 54.6842f;
    tmpConfig.gps_lon         = obj["gps_lon"]         | 25.2398f;
    tmpConfig.gps_alt         = obj["gps_alt"]         | 10.0f;
    tmpConfig.aprs_ssid       = obj["aprs_ssid"]       | (uint8_t)15;
    tmpConfig.aprs_port       = obj["aprs_port"]       | (uint16_t)14580;
    tmpConfig.aprs_moniSSID   = obj["aprs_moniSSID"]   | (uint8_t)0;
    tmpConfig.api_id          = obj["api_id"]          | (uint32_t)0;
    tmpConfig.tnc             = obj["tnc"]             | false;
    tmpConfig.rf2inet         = obj["rf2inet"]         | false;
    tmpConfig.inet2rf         = obj["inet2rf"]         | false;
    tmpConfig.tnc_digi        = obj["tnc_digi"]        | false;
    tmpConfig.tnc_telemetry   = obj["tnc_telemetry"]   | false;
    tmpConfig.tnc_beacon      = obj["tnc_beacon"]      | 0;
    tmpConfig.aprs_beacon     = obj["aprs_beacon"]     | 10;
    tmpConfig.aprs_table      = (char)(obj["aprs_table"]  | (uint8_t)'/');
    tmpConfig.aprs_symbol     = (char)(obj["aprs_symbol"] | (uint8_t)'&');
    strlcpy(tmpConfig.aprs_mycall,   obj["aprs_mycall"]   | "MYCALL",          sizeof(tmpConfig.aprs_mycall));
    strlcpy(tmpConfig.aprs_host,     obj["aprs_host"]     | "rotate.aprs2.net",sizeof(tmpConfig.aprs_host));
    strlcpy(tmpConfig.aprs_passcode, obj["aprs_passcode"] | "00000",           sizeof(tmpConfig.aprs_passcode));
    strlcpy(tmpConfig.aprs_moniCall, obj["aprs_moniCall"] | "",                sizeof(tmpConfig.aprs_moniCall));
    strlcpy(tmpConfig.aprs_filter,   obj["aprs_filter"]   | "m/50",            sizeof(tmpConfig.aprs_filter));
    strlcpy(tmpConfig.aprs_comment,  obj["aprs_comment"]  | "",                sizeof(tmpConfig.aprs_comment));
    strlcpy(tmpConfig.aprs_path,     obj["aprs_path"]     | "WIDE1-1",         sizeof(tmpConfig.aprs_path));
    strlcpy(tmpConfig.wifi_ssid,     obj["wifi_ssid"]     | "APRS-ESP32",      sizeof(tmpConfig.wifi_ssid));
    strlcpy(tmpConfig.wifi_pass,     obj["wifi_pass"]     | "aprs-esp32",      sizeof(tmpConfig.wifi_pass));
    strlcpy(tmpConfig.wifi_ap_ssid,  obj["wifi_ap_ssid"]  | "APRS-ESP32",      sizeof(tmpConfig.wifi_ap_ssid));
    strlcpy(tmpConfig.wifi_ap_pass,  obj["wifi_ap_pass"]  | "aprs-esp32",      sizeof(tmpConfig.wifi_ap_pass));
    strlcpy(tmpConfig.tnc_path,      obj["tnc_path"]      | "WIDE1-1",         sizeof(tmpConfig.tnc_path));
    strlcpy(tmpConfig.tnc_btext,     obj["tnc_btext"]     | "",                sizeof(tmpConfig.tnc_btext));
    strlcpy(tmpConfig.tnc_comment,   obj["tnc_comment"]   | "",                sizeof(tmpConfig.tnc_comment));
    strlcpy(tmpConfig.aprs_object,   obj["aprs_object"]   | "",                sizeof(tmpConfig.aprs_object));
    tmpConfig.wifi_power      = obj["wifi_power"]      | (uint8_t)44;
    tmpConfig.tx_timeslot     = obj["tx_timeslot"]     | (uint16_t)5000;
    tmpConfig.digi_delay      = obj["digi_delay"]      | (uint16_t)2000;
    tmpConfig.input_hpf       = obj["input_hpf"]       | false;
    tmpConfig.freq_rx         = obj["freq_rx"]         | 144.8000f;
    tmpConfig.freq_tx         = obj["freq_tx"]         | 144.8000f;
    tmpConfig.offset_rx       = obj["offset_rx"]       | 0;
    tmpConfig.offset_tx       = obj["offset_tx"]       | 0;
    tmpConfig.tone_rx         = obj["tone_rx"]         | 0;
    tmpConfig.tone_tx         = obj["tone_tx"]         | 0;
    tmpConfig.band            = obj["band"]            | (uint8_t)1;
    tmpConfig.sql_level       = obj["sql_level"]       | (uint8_t)1;
    tmpConfig.rf_power        = obj["rf_power"]        | false;
    tmpConfig.volume          = obj["volume"]          | (uint8_t)4;
    tmpConfig.rx_att          = obj["rx_att"]          | false;
    tmpConfig.timeZone        = obj["timeZone"]        | (int8_t)0;
    strlcpy(tmpConfig.ntpServer, obj["ntpServer"] | "pool.ntp.org", sizeof(tmpConfig.ntpServer));
    tmpConfig.gps_mode        = obj["gps_mode"]        | (uint8_t)GPS_MODE_FIXED;
    tmpConfig.sb_fast_speed   = obj["sb_fast_speed"]   | (uint16_t)APRS_SB_FAST_SPEED;
    tmpConfig.sb_fast_rate    = obj["sb_fast_rate"]    | (uint16_t)APRS_SB_FAST_RATE;
    tmpConfig.sb_slow_speed   = obj["sb_slow_speed"]   | (uint16_t)APRS_SB_SLOW_SPEED;
    tmpConfig.sb_slow_rate    = obj["sb_slow_rate"]    | (uint16_t)APRS_SB_SLOW_RATE;
    tmpConfig.sb_turn_min     = obj["sb_turn_min"]     | (uint16_t)APRS_SB_MIN_TURN_ANGLE;
    tmpConfig.sb_turn_slope   = obj["sb_turn_slope"]   | (uint16_t)APRS_SB_TURN_SLOPE;
    tmpConfig.sb_turn_time    = obj["sb_turn_time"]    | (uint16_t)APRS_SB_MIN_TURN_TIME;
    tmpConfig.oled_brightness     = obj["oled_brightness"]     | (uint8_t)255;
    tmpConfig.locator_popup       = obj["locator_popup"]       | (uint8_t)0;
    tmpConfig.oled_autodim        = obj["oled_autodim"]        | (uint8_t)0;
    tmpConfig.oled_autodim_level  = obj["oled_autodim_level"]  | (uint8_t)1;
    tmpConfig.aprs_rx_popup       = obj["aprs_rx_popup"]       | (uint8_t)3;
    tmpConfig.aprs_sms_popup      = obj["aprs_sms_popup"]      | (uint8_t)60;
    tmpConfig.aprs_sms_rx         = obj["aprs_sms_rx"]         | true;

    return tmpConfig;
}
