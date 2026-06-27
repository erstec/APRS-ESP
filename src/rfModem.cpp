/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the RF modem control.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate                    
*/

#include "rfModem.h"
#include "config.h"
#include "main.h"
#include "oled.h"

extern HardwareSerial SerialRF;
extern Configuration config;

unsigned long SA818_Timeout = 0;

bool rfAnswerCheck(void) {
    if (SerialRF.available() > 0) {
        String ret = SerialRF.readString();
        log_i("->%s", ret.c_str());
        if (ret.indexOf(":0") > 0) {
#ifdef DEBUG
            log_i("SA Answer OK");
#endif
            return true;
        }
        return false;
    } else {
        log_e("SA Answer Error");
        return false;
    }
}

static bool rfWaitResponse(uint32_t timeoutMs = 600) {
    unsigned long deadline = millis() + timeoutMs;
    String ret;
    while (millis() < deadline) {
        if (SerialRF.available()) {
            ret += (char)SerialRF.read();
            if (ret.indexOf(":0") > 0) { log_i("->%s", ret.c_str()); return true; }
            if (ret.indexOf(":1") > 0) { log_i("->%s", ret.c_str()); return false; }
        }
    }
    log_e("->timeout: %s", ret.c_str());
    return false;
}

static bool rfSendCmd(const char *cmd, int retries = 5) {
    for (int i = 0; i < retries; i++) {
        while (SerialRF.available()) SerialRF.read();
        SerialRF.println(cmd);
        if (rfWaitResponse()) return true;
        log_w("rfSendCmd retry %d: %s", i + 1, cmd);
    }
    return false;
}

bool RF_Init(bool boot, bool showOled = false) {
    log_i("SA818/SA868 Init");
#ifdef USE_SCREEN
    OledPostStartup(0);
#endif
    if (boot) {
        SerialRF.begin(SERIAL_RF_BAUD, SERIAL_8N1, SERIAL_RF_RXPIN, SERIAL_RF_TXPIN);

        pinMode(POWERDOWN_PIN, OUTPUT);
        digitalWrite(POWERDOWN_PIN, HIGH);

        pinMode(POWER_PIN, OUTPUT);
        digitalWrite(POWER_PIN, LOW);

        if (SQL_PIN > -1)
            pinMode(SQL_PIN, INPUT_PULLUP);
    }

    // SA868 needs up to ~6s from power-on before responding; poll until ready
    bool ready = false;
    for (int i = 0; i < 15; i++) {
        while (SerialRF.available()) SerialRF.read();
        SerialRF.println("AT+DMOCONNECT");
        if (rfWaitResponse(500)) { ready = true; break; }
        log_w("DMOCONNECT attempt %d failed", i + 1);
    }
    if (!ready) {
        log_e("SA868 not responding after 15 attempts");
        return false;
    }
    log_i("SA868 ready");
    char str[100];
    if (config.sql_level > 8) config.sql_level = 8;

#ifdef USE_SCREEN
    if (showOled) OledPostStartup(1);
#endif

    sprintf(str, "AT+DMOSETGROUP=%01d,%0.4f,%0.4f,%04d,%01d,%04d",
            config.band,
            config.freq_tx + ((float)config.offset_tx / 1000000),
            config.freq_rx + ((float)config.offset_rx / 1000000),
            config.tone_tx, config.sql_level, config.tone_rx);
    log_i("%s", str);
    if (!rfSendCmd(str)) return false;

#ifdef USE_SCREEN
    if (showOled) OledPostStartup(2);
#endif

    log_i("AT+SETFILTER=1,1,1");
    if (!rfSendCmd("AT+SETFILTER=1,1,1")) return false;

#ifdef USE_SCREEN
    if (showOled) OledPostStartup(3);
#endif

    log_i("AT+SETTAIL=0");
    if (!rfSendCmd("AT+SETTAIL=0")) return false;

#ifdef USE_SCREEN
    if (showOled) OledPostStartup(4);
#endif

    if (config.volume > 8) config.volume = 8;
    char volCmd[24];
    snprintf(volCmd, sizeof(volCmd), "AT+DMOSETVOLUME=%d", config.volume);
    log_i("%s", volCmd);
    if (!rfSendCmd(volCmd)) return false;

    return true;
}

void RF_Check() {
    while (SerialRF.available() > 0) {
        SerialRF.read();
    }

    SerialRF.println("AT+DMOCONNECT");
    log_i("AT+DMOCONNECT");
    delay(100);
    if (rfAnswerCheck()) {
        SA818_Timeout = millis();
    } else {
        pinMode(POWER_PIN, OUTPUT);
        digitalWrite(POWER_PIN, LOW);
        digitalWrite(POWERDOWN_PIN, LOW);
        delay(500);
        RF_Init(true);
    }
}
