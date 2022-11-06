/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the RF modem control.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate                    
*/

#include "rfModem.h"
#include "config.h"
#include "main.h"

extern HardwareSerial SerialRF;
extern Configuration config;

unsigned long SA818_Timeout = 0;

bool rfAnswerCheck(void) {
#ifdef USE_SA828
    return true;
#else
    if (SerialRF.available() > 0) {
        String ret = SerialRF.readString();
        Serial.print("->" + ret);
        if (ret.indexOf(":0") > 0) {
            // SA818_Timeout = millis();
#ifdef DEBUG
            Serial.println("SA Answer OK");
#endif
            return true;
        }
        return false;
    } else {
        Serial.println("SA Answer Error");
        return false;
    }
#endif
}

bool RF_Init(bool boot) {
#if defined(USE_SR_FRS)
    Serial.println("SR_FRS Init");
#elif defined(USE_SA818)
    Serial.println("SA818 Init");
#elif defined(USE_SA868)
    Serial.println("SA868 Init");
#elif defined(USE_SA828)
    Serial.println("SA828 Init");
#endif
    if (boot) {
        SerialRF.begin(SERIAL_RF_BAUD, SERIAL_8N1, SERIAL_RF_RXPIN, SERIAL_RF_TXPIN);
        
        pinMode(POWER_PIN, OUTPUT_OPEN_DRAIN);
        pinMode(POWERDOWN_PIN, OUTPUT);
        pinMode(SQL_PIN, INPUT_PULLUP);

        digitalWrite(POWER_PIN, LOW);
        digitalWrite(POWERDOWN_PIN, LOW);
        delay(500);
        digitalWrite(POWERDOWN_PIN, HIGH);
        delay(1500);
        Serial.println("RF Modem powered up");
#if !defined(USE_SA828)
        SerialRF.println();
        delay(500);
        while (SerialRF.available()) {
            char c = SerialRF.read();
            Serial.print(c);
        }
#endif
    }
#if !defined(USE_SA828)
    SerialRF.println();
    delay(500);
    while (SerialRF.available()) {
        char c = SerialRF.read();
        Serial.print(c);
    }
#endif
    // #if defined(USE_SA828)
    // char str[512];
    // #else
    char str[100];
    // #endif
    if (config.sql_level > 8) config.sql_level = 8;
#if defined(SR_FRS)
    sprintf(str, "AT+DMOSETGROUP=%01d,%0.4f,%0.4f,%d,%01d,%d,0", config.band,
            config.freq_tx + ((float)config.offset_tx / 1000000),
            config.freq_rx + ((float)config.offset_rx / 1000000),
            config.tone_rx, config.sql_level, config.tone_tx);
    SerialRF.println(str);
    delay(500);
    // Module auto power save setting
    SerialRF.println("AT+DMOAUTOPOWCONTR=1");
    delay(500);
    SerialRF.println("AT+DMOSETVOX=0");
    delay(500);
    SerialRF.println("AT+DMOSETMIC=1,0,0");
#elif defined(USE_SA818)
    sprintf(str, "AT+DMOSETGROUP=%01d,%0.4f,%0.4f,%04d,%01d,%04d", 
            config.band,
            config.freq_tx + ((float)config.offset_tx / 1000000),
            config.freq_rx + ((float)config.offset_rx / 1000000),
            config.tone_tx, config.sql_level, config.tone_rx);
    SerialRF.println(str);
    Serial.println(str);
    delay(500);
    SerialRF.println("AT+SETTAIL=0");
    Serial.println("AT+SETTAIL=0");
    delay(500);
    SerialRF.println("AT+SETFILTER=1,1,1");
    Serial.println("AT+SETFILTER=1,1,1");
#elif defined(USE_SA868)
    //sprintf(str, "\r\n\r\n\r\nAT+DMOSETGROUP=%01d,%0.4f,%0.4f,%04d,%01d,%04d", 
    sprintf(str, "AT+DMOSETGROUP=%01d,%0.4f,%0.4f,%04d,%01d,%04d", 
            config.rf_power == 1 ? 0 : 1,   // 0: High power, 1: Low power
            config.freq_tx + ((float)config.offset_tx / 1000000),
            config.freq_rx + ((float)config.offset_rx / 1000000),
            config.tone_tx, config.sql_level, config.tone_rx);
    SerialRF.println(str);
    Serial.println(str);
    delay(500);
    if (!rfAnswerCheck()) return false;

    SerialRF.println("AT+SETFILTER=1,1,1");
    Serial.println("AT+SETFILTER=1,1,1");
#elif defined(USE_SA828)
    // int idx = sprintf(str, "AAFA3");
    // for (uint8_t i = 0; i < 16; i++) {
    //     idx += sprintf(&str[idx], "%0.4f,", config.freq_tx +
    //     ((float)config.offset_tx / 1000000)); idx += sprintf(&str[idx],
    //     "%0.4f,", config.freq_rx + ((float)config.offset_rx / 1000000));
    // }
    // idx += sprintf(&str[idx], "%03d,%03d,%d", config.tone_tx, config.tone_rx,
    // config.sql_level); SerialRF.println(str);
    SerialRF.print("AAFA3");
#ifdef DEBUG_RF
    Serial.print("AAFA3");
#endif
    for (uint8_t i = 0; i < 16; i++) {
        int idx = sprintf(str, "%0.4f,",
                          config.freq_tx + ((float)config.offset_tx / 1000000));
        sprintf(&str[idx], "%0.4f,",
                config.freq_rx + ((float)config.offset_rx / 1000000));
        SerialRF.print(str);
#ifdef DEBUG_RF
        Serial.print(str);
#endif
    }
    sprintf(str, "%03d,%03d,%d", config.tone_tx, config.tone_rx,
            config.sql_level);
    SerialRF.println(str);
#ifdef DEBUG_RF
    Serial.println(str);
#endif
#endif
    // SerialRF.println(str);
    delay(500);
    if (!rfAnswerCheck()) return false;

    if (config.volume > 8) config.volume = 8;
#if !defined(USE_SA828)
    SerialRF.printf("AT+DMOSETVOLUME=%d\r\n", config.volume);
    Serial.printf("AT+DMOSETVOLUME=%d\r\n", config.volume);
    delay(500);
    if (!rfAnswerCheck()) return false;
#endif
    return true;
}

void RF_Sleep() {
    digitalWrite(POWER_PIN, LOW);
    digitalWrite(POWERDOWN_PIN, LOW);
    // SerialGPS.print("$PMTK161,0*28\r\n");
    // AFSK_TimerEnable(false);
}

void RF_Check() {
    while (SerialRF.available() > 0) {
        SerialRF.read();
    }
#if !defined(USE_SA828)
    SerialRF.println("AT+DMOCONNECT");
    Serial.println("AT+DMOCONNECT");
    delay(100);
    if (rfAnswerCheck()) {
        SA818_Timeout = millis();
    } else {
        digitalWrite(POWER_PIN, LOW);
        digitalWrite(POWERDOWN_PIN, LOW);
        delay(500);
        RF_Init(true);
    }
    // SerialGPS.print("$PMTK161,0*28\r\n");
    // AFSK_TimerEnable(false);
#endif
}

// unsigned long SA818_Timeout = 0;
// void RF_Init(uint8_t HL)
// {

//     pinMode(0, INPUT);
//     pinMode(POWER_PIN, OUTPUT);
//     pinMode(POWERDOWN_PIN, OUTPUT);
//     pinMode(SQL_PIN, INPUT_PULLUP);

//     SerialRF.begin(9600, SERIAL_8N1, 14, 13);

//     digitalWrite(POWERDOWN_PIN, HIGH);
//     digitalWrite(POWER_PIN, LOW);
//     delay(500);
//     // AT+DMOSETGROUP=1,144.3900,144.3900,0,1,0,0
//     SerialRF.println("AT+DMOSETGROUP=0,144.3900,144.3900,0,1,0,0");
//     delay(10);
//     SerialRF.println("AT+DMOAUTOPOWCONTR=1");
//     delay(10);
//     SerialRF.println("AT+DMOSETVOLUME=9");
//     delay(10);
//     SerialRF.println("AT+DMOSETVOX=0");
//     delay(10);
//     SerialRF.println("AT+DMOSETMIC=8,0,0");
//     delay(100);
//     // AFSK_TimerEnable(true);
//     digitalWrite(POWER_PIN, HL);
// }

// void RF_Sleep()
// {
//     digitalWrite(POWER_PIN, LOW);
//     digitalWrite(POWERDOWN_PIN, LOW);
//     // SerialGPS.print("$PMTK161,0*28\r\n");
//     // AFSK_TimerEnable(false);
// }

// void RF_Check()
// {
//     while (SerialRF.available() > 0)
//         SerialRF.read();
//     SerialRF.println("AT+DMOCONNECT");
//     delay(100);
//     if (SerialRF.available() > 0)
//     {
//         String ret = SerialRF.readString();
//         if (ret.indexOf("DMOCONNECT") > 0)
//         {
//             SA818_Timeout = millis();
// #ifdef DEBUG
//             // Serial.println(SerialRF.readString());
//             Serial.println("SA818 Activated");
// #endif
//         }
//     }
//     else
//     {
//         Serial.println("SA818 deActive");
//         digitalWrite(POWER_PIN, LOW);
//         digitalWrite(POWERDOWN_PIN, LOW);
//         delay(500);
//         SA818_INIT(LOW);
//     }
//     // SerialGPS.print("$PMTK161,0*28\r\n");
//     // AFSK_TimerEnable(false);
// }
