/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the IGate functionality.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "igate.h"
#include "config.h"

extern WiFiClient aprsClient;
extern Configuration config;

#define IGATE_DUPE_SIZE 50
#define IGATE_DUPE_SECS 30

static struct { char key[14]; time_t lastGated; } igateDupe[IGATE_DUPE_SIZE];

static bool igateCheckDupe(const char *call, char pktType) {
    char key[14] = {0};
    snprintf(key, sizeof(key), "%s\x01%c", call, pktType);
    time_t now = time(NULL);
    int oldest = 0;
    for (int i = 0; i < IGATE_DUPE_SIZE; i++) {
        if (strcmp(igateDupe[i].key, key) == 0) {
            if (now - igateDupe[i].lastGated < IGATE_DUPE_SECS) return true;
            igateDupe[i].lastGated = now;
            return false;
        }
        if (igateDupe[i].lastGated < igateDupe[oldest].lastGated) oldest = i;
    }
    strlcpy(igateDupe[oldest].key, key, sizeof(igateDupe[oldest].key));
    igateDupe[oldest].lastGated = now;
    return false;
}

int igateProcess(AX25Msg &Packet) {
    int idx;
    String header;

    if (Packet.len < 2) {
        // digiLog.ErPkts++;
        return 0;  // NO INFO DATA
    }

    if (!strcmp(&Packet.src.call[0], &config.aprs_mycall[0]) && Packet.src.ssid == config.aprs_ssid) {
        return 0;
    }

    char srcCall[12] = {0};
    if (Packet.src.ssid > 0)
        snprintf(srcCall, sizeof(srcCall), "%s-%d", Packet.src.call, Packet.src.ssid);
    else
        strlcpy(srcCall, Packet.src.call, sizeof(srcCall));

    if (igateCheckDupe(srcCall, Packet.len > 0 ? (char)Packet.info[0] : 0)) return 0;

    for (idx = 0; idx < Packet.rpt_count; idx++) {
        if (!strncmp(&Packet.rpt_list[idx].call[0], "RFONLY", 6)) {
            // digiLog.DropRx++;
            return 0;
        }
    }

    for (idx = 0; idx < Packet.rpt_count; idx++) {
        if (!strncmp(&Packet.rpt_list[idx].call[0], "NOGATE", 6)) {
            // digiLog.DropRx++;
            return 0;
        }
    }  

    for (idx = 0; idx < Packet.rpt_count; idx++) {
        if (!strncmp(&Packet.rpt_list[idx].call[0], "TCPIP", 5)) {
            // digiLog.DropRx++;
            return 0;
        }
    }

    header = String(Packet.src.call);
    if (Packet.src.ssid > 0) {
        header += String(F("-"));
        header += String(Packet.src.ssid);
    }
    header += String(F(">"));
    header += String(Packet.dst.call);
    if (Packet.dst.ssid > 0) {
        header += String(F("-"));
        header += String(Packet.dst.ssid);
    }

    // Add Path
    for (int i = 0; i < Packet.rpt_count; i++) {
        header += String(",");
        header += String(Packet.rpt_list[i].call);
        if (Packet.rpt_list[i].ssid > 0) {
            header += String("-");
            header += String(Packet.rpt_list[i].ssid);
        }
        if (Packet.rpt_flags & (1 << i)) header += "*";
    }

    // Add qAR,callSSID
    header += String(F(",qAR,"));
    if (strlen(config.aprs_object) >= 3) {
        header += String(config.aprs_object);
    } else {
        header += String(config.aprs_mycall);
        if (config.aprs_ssid > 0) {
            header += String(F("-"));
            header += String(config.aprs_ssid);
        }
    }

    // Add Information
    header += String(F(":"));
    size_t hSize = strlen(header.c_str());
    uint8_t raw[500];
    memset(raw, 0, sizeof(raw));
    memcpy(&raw[0], header.c_str(), hSize);

    // Prevent multi lines (Issue #48)
    size_t crlf_pos = Packet.len;
    for (int i = 0; i < Packet.len; i++) {
        if (Packet.info[i] == '\n' || Packet.info[i] == '\r') {
            crlf_pos = i;
            break;
        }
    }

    memcpy(&raw[hSize], &Packet.info[0], crlf_pos);
    aprsClient.write(&raw[0], hSize + crlf_pos);
    aprsClient.write("\r\n");

    return 1;
}
