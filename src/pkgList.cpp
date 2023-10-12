/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for various common functions.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "pkgList.h"
#include "parse_aprs.h"
#include "main.h"

pkgListType pkgList[PKGLISTSIZE];

char pkgList_Find(char *call) {
    char i;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if (strstr(pkgList[(int)i].calsign, call) != NULL) return i;
    }
    return -1;
}

char pkgListOld() {
    char i, ret = 0;
    time_t minimum = pkgList[0].time;
    for (i = 1; i < PKGLISTSIZE; i++) {
        if (pkgList[(int)i].time < minimum) {
            minimum = pkgList[(int)i].time;
            ret = i;
        }
    }
    return ret;
}

void sort(pkgListType a[], int size) {
    pkgListType t;
    char *ptr1;
    char *ptr2;
    char *ptr3;
    ptr1 = (char *)&t;
    for (int i = 0; i < (size - 1); i++) {
        for (int o = 0; o < (size - (i + 1)); o++) {
            if (a[o].time < a[o + 1].time) {
                ptr2 = (char *)&a[o];
                ptr3 = (char *)&a[o + 1];
                memcpy(ptr1, ptr2, sizeof(pkgListType));
                memcpy(ptr2, ptr3, sizeof(pkgListType));
                memcpy(ptr3, ptr1, sizeof(pkgListType));
            }
        }
    }
}

void sortPkgDesc(pkgListType a[], int size) {
    pkgListType t;
    char *ptr1;
    char *ptr2;
    char *ptr3;
    ptr1 = (char *)&t;
    for (int i = 0; i < (size - 1); i++) {
        for (int o = 0; o < (size - (i + 1)); o++) {
            if (a[o].pkg < a[o + 1].pkg) {
                ptr2 = (char *)&a[o];
                ptr3 = (char *)&a[o + 1];
                memcpy(ptr1, ptr2, sizeof(pkgListType));
                memcpy(ptr2, ptr3, sizeof(pkgListType));
                memcpy(ptr3, ptr1, sizeof(pkgListType));
            }
        }
    }
}

uint8_t pkgType(const char *raw) {
    uint8_t type = 0;
    char packettype = 0;
    const char *info_start, *body;
    int paclen = strlen(raw);
    // info_start = (char*)strchr(raw, ':');
    // if (info_start == NULL) return 0;
    // info_start=0;
    packettype = (char)raw[0];
    body = &raw[1];

    switch (packettype) {
    case '=':
    case '/':
    case '@':
        if (strchr(body, 'r') != NULL) {
            if (strchr(body, 'g') != NULL) {
                if (strchr(body, 't') != NULL) {
                    if (strchr(body, 'P') != NULL) {
                        type = PKG_WX;
                    }
                }
            }
        }
        break;
    case ':':
        type = PKG_MESSAGE;
        if (body[9] == ':' &&
            (memcmp(body + 9, ":PARM.", 6) == 0 ||
             memcmp(body + 9, ":UNIT.", 6) == 0 ||
             memcmp(body + 9, ":EQNS.", 6) == 0 ||
             memcmp(body + 9, ":BITS.", 6) == 0))
        {
            type = PKG_TELEMETRY;
        }
        break;
    case '>':
        type = PKG_STATUS;
        break;
    case '?':
        type = PKG_QUERY;
        break;
    case ';':
        type = PKG_OBJECT;
        break;
    case ')':
        type = PKG_ITEM;
        break;
    case 'T':
        type = PKG_TELEMETRY;
        break;
    case '#': /* Peet Bros U-II Weather Station */
    case '*': /* Peet Bros U-I  Weather Station */
    case '_': /* Weather report without position */
        type = PKG_WX;
        break;
    default:
        type = 0;
        break;
    }
    return type;
}

void pkgListUpdate(char *call, uint8_t type) {
    char i = pkgList_Find(call);
    time_t now;
    time(&now);
    if (i != 255) {  // Found call in old pkg
        pkgList[(uint)i].time = now;
        pkgList[(uint)i].pkg++;
        pkgList[(uint)i].type = type;
        // Serial.print("Update: ");
    } else {
        i = pkgListOld();
        pkgList[(uint)i].time = now;
        pkgList[(uint)i].pkg = 1;
        pkgList[(uint)i].type = type;
        strcpy(pkgList[(uint)i].calsign, call);
        // strcpy(pkgList[(uint)i].ssid, &ssid[0]);
        pkgList[(uint)i].calsign[10] = 0;
        // Serial.print("NEW: ");
    }
}
