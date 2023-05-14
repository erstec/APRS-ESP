/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with Configuration, stored in EEPROM.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

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

void SaveConfig() {
    uint8_t chkSum = 0;
    byte *ptr;
    ptr = (byte *)&config;
    EEPROM.writeBytes(1, ptr, sizeof(Configuration));
    chkSum = checkSum(ptr, sizeof(Configuration));
    EEPROM.write(0, chkSum);
    EEPROM.commit();
#ifdef DEBUG
    Serial.print("Save EEPROM ChkSUM=");
    Serial.println(chkSum, HEX);
#endif
    SPIFFS.begin(true);
    File f = SPIFFS.open("/config.bin", "w");
    if (!f) {
        Serial.println("Failed to open config file for writing");
        SPIFFS.end();
        return;
    }
    f.write(chkSum);
    f.write((byte *)&config, sizeof(Configuration));
    f.close();
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
    sprintf(config.aprs_filter, "g/HS*/E2*");
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
    config.band = 0;
    config.sql_level = 1;
    config.rf_power = LOW;
    config.volume = 4;
    config.input_hpf = false;
#endif
    input_HPF = config.input_hpf;
    config.timeZone = 0;
    SaveConfig();
}

void LoadConfig() {
    byte *ptr;

    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.println(F("failed to initialise EEPROM"));  // delay(100000);
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
    Serial.printf("EEPROM Check %0Xh=%0Xh(%dByte)\r\n", EEPROM.read(0), chkSum,
                  sizeof(Configuration));
    if (EEPROM.read(0) != chkSum) {
        Serial.println("Config EEPROM Error!");
        DefaultConfig();
    }
    input_HPF = config.input_hpf;
}

void LoadReConfig() {
    byte *ptr;

    SPIFFS.begin(true);
    File f = SPIFFS.open("/config.bin", "r");
    if (!f) {
        Serial.println("Failed to open config file for reading");
        SPIFFS.end();
        return;
    }
    
    Configuration tmpConfig;

    uint8_t chkSum = f.read();
    size_t sz = f.read((byte *)&tmpConfig, sizeof(Configuration));
    f.close();
    SPIFFS.end();

    if (sz != sizeof(Configuration)) {
        Serial.println("Config File Error!");
        return;
    }

    ptr = (byte *)&tmpConfig;

    uint8_t chkSum2 = checkSum(ptr, sizeof(Configuration));
    Serial.printf("SPIFFS Check %0Xh=%0Xh(%dByte)\r\n", chkSum, chkSum2, sizeof(Configuration));

    if (chkSum == chkSum2) {
        memcpy(&config, &tmpConfig, sizeof(Configuration));
        input_HPF = config.input_hpf;
        SaveConfig();
    }
}
