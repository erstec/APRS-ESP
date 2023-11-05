/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for various common functions.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "pkgList.h"
#include "main.h"

pkgListType *pkgList;

extern int mVrms;

char pkgList_Find(char *call) {
    char i;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if (strstr(pkgList[(int)i].calsign, call) != NULL) return i;
    }
    return -1;
}

int pkgList_Find(char *call, uint16_t type) {
    int i;
    for (i = 0; i < PKGLISTSIZE; i++) {
        if ((strstr(pkgList[i].calsign, call) != NULL) && (pkgList[i].type == type))
            return i;
    }
    return -1;
}

char pkgListOld() {
    char i, ret = 0;
    time_t minimum = time(NULL) + 86400;
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
    while (psramBusy)
        delay(1);
    psramBusy = true;
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
    psramBusy = false;
}

void sortPkgDesc(pkgListType a[], int size) {
    pkgListType t;
    char *ptr1;
    char *ptr2;
    char *ptr3;
    ptr1 = (char *)&t;
    while (psramBusy)
        delay(1);
    psramBusy = true;
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
    psramBusy = false;
}

uint16_t pkgType(const char *raw) {
    uint16_t type = 0;
    char packettype = 0;
    const char *body;

    if (*raw == 0)
        return 0;

    packettype = (char)raw[0];
    body = &raw[1];

    switch (packettype) {
        case '$': // NMEA
            type |= FILTER_POSITION;
            break;
        case 0x27: /* ' */
        case 0x60: /* ` */
            type |= FILTER_POSITION;
            type |= FILTER_MICE;
            break;
        case '!':
        case '=':
            type |= FILTER_POSITION;
            if (body[18] == '_' || body[10] == '_') {
                type |= FILTER_WX;
                break;
            }
        case '/':
        case '@':
            type |= FILTER_POSITION;
            if (body[25] == '_' || body[16] == '_') {
                type |= FILTER_WX;
                break;
            }
            if (strchr(body, 'r') != NULL) {
                if (strchr(body, 'g') != NULL) {
                    if (strchr(body, 't') != NULL) {
                        if (strchr(body, 'P') != NULL) {
                            type |= FILTER_WX;
                        }
                    }
                }
            }
            break;
        case ':':
            if (body[9] == ':' &&
                (memcmp(body + 10, "PARM", 4) == 0 ||
                memcmp(body + 10, "UNIT", 4) == 0 ||
                memcmp(body + 10, "EQNS", 4) == 0 ||
                memcmp(body + 10, "BITS", 4) == 0)) {
                    type |= FILTER_TELEMETRY;
            } else {
                type |= FILTER_MESSAGE;
            }
            break;
        case '{': // User defind
        case '<': // statcapa
        case '>':
            type |= FILTER_STATUS;
            break;
        case '?':
            type |= FILTER_QUERY;
            break;
        case ';':
            if (body[28] == '_')
                type |= FILTER_WX;
            else
                type |= FILTER_OBJECT;
            break;
        case ')':
            type |= FILTER_ITEM;
            break;
        case '}':
            type |= FILTER_THIRDPARTY;
            break;
        case 'T':
            type |= FILTER_TELEMETRY;
            break;
        case '#': /* Peet Bros U-II Weather Station */
        case '*': /* Peet Bros U-I  Weather Station */
        case '_': /* Weather report without position */
            type |= FILTER_WX;
            break;
        default:
            type = 0;
            break;
    }
    return type;
}

pkgListType getPkgList(int idx) {
    pkgListType ret;
    while (psramBusy)
        delay(1);
    psramBusy = true;
    memcpy(&ret, &pkgList[idx], sizeof(pkgListType));
    psramBusy = false;
    
    return ret;
}

int pkgListUpdate(char *call, char *raw, uint16_t type, bool channel) {
    size_t len;
    if (*call == 0)
        return -1;
    if (*raw == 0)
        return -1;

    char callsign[11];
    size_t sz = strlen(call);
    memset(callsign, 0, sizeof(callsign));
    if (sz > (sizeof(callsign) - 1))
        sz = sizeof(callsign) - 1;
    // strncpy(callsign, call, sz);
    memcpy(callsign, call, sz);

    log_d("call: %s, callsign: %s, sz: %d", call, callsign, sz);

    while (psramBusy)
        delay(1);
    psramBusy = true;

    int i = pkgList_Find(callsign, type);
    if (i > PKGLISTSIZE) {
        psramBusy = false;
        return -1;
    }
    if (i > -1) { // Found call in old pkg
        if (channel == pkgList[i].channel) {
            pkgList[i].time = time(NULL);
            pkgList[i].pkg++;
            pkgList[i].type = type;
            if (channel == 0)
                pkgList[i].audio_level = (int16_t)mVrms;
            else
                pkgList[i].audio_level = 0;
            len = strlen(raw);
            if (len > sizeof(pkgList[i].raw))
                len = sizeof(pkgList[i].raw);
            memcpy(pkgList[i].raw, raw, len);
            // SerialLOG.print("Update: ");

            log_d("Update: %s, i: %d", pkgList[i].calsign, i);
        }
    } else {
        i = pkgListOld(); // Search free in array
        if (i > PKGLISTSIZE || i < 0) {
            psramBusy = false;
            return -1;
        }
        // memset(&pkgList[i], 0, sizeof(pkgListType));
        pkgList[i].channel = channel;
        pkgList[i].time = time(NULL);
        pkgList[i].pkg = 1;
        pkgList[i].type = type;
        if (channel == 0)
            pkgList[i].audio_level = (int16_t)mVrms;
        else
            pkgList[i].audio_level = 0;
        // strcpy(pkgList[i].calsign, callsign);
        memset(pkgList[i].calsign, 0, sizeof(pkgList[i].calsign));
        memcpy(pkgList[i].calsign, callsign, strlen(callsign));
        pkgList[i].calsign[sizeof(callsign) - 1] = 0;
        len = strlen(raw);
        if (len > sizeof(pkgList[i].raw))
            len = sizeof(pkgList[i].raw);
        memset(pkgList[i].raw, 0, sizeof(pkgList[i].raw));
        memcpy(pkgList[i].raw, raw, len);
        // strcpy(pkgList[i].raw, raw);
        // SerialLOG.print("NEW: ");

        log_d("NEW: %s, i: %d", pkgList[i].calsign, i);
    }
    psramBusy = false;
    
    return i;
}

void pkgListInit() {
#ifdef BOARD_HAS_PSRAM
    pkgList = (pkgListType *)ps_malloc(sizeof(pkgListType) * PKGLISTSIZE);
#else
    pkgList = (pkgListType *)malloc(sizeof(pkgListType) * PKGLISTSIZE);
#endif
    memset(pkgList, 0, sizeof(pkgListType) * PKGLISTSIZE);
}
