#pragma once
#include <Arduino.h>
#include <time.h>

#define APRS_MSG_INBOX_SIZE 10

struct AprsMsg {
    char   from[12];
    char   text[70];
    char   msgid[6];
    time_t when;
};

void    aprsMsgAdd(const char *from, const char *text, const char *msgid);
AprsMsg aprsMsgGet(int idx);
int     aprsMsgCount();
