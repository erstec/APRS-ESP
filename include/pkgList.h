#ifndef PKGLIST_H
#define PKGLIST_H

#include <Arduino.h>

typedef struct pkgListStruct {
    time_t time;
    char calsign[11];
    char ssid[5];
    unsigned int pkg;
    bool type;
    uint8_t symbol;
} pkgListType;

void sort(pkgListType a[], int size);
void sortPkgDesc(pkgListType a[], int size);
void pkgListUpdate(char *call, bool type);

#endif // PKGLIST_H
