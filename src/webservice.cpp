/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for the Web Service functionality.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/
#include "AFSK.h"
#include "webservice.h"
#include "oled.h"
#include "base64.hpp"
#include "utilities.h"
#include <LibAPRSesp.h>
#include <TinyGPS++.h>
#include "gps.h"
#include <vector>
#include <utility>
#include <ArduinoJson.h>

WebServer server(80);
DNSServer dnsServer;


static void wsBegin() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "");
}
static void ws(const char *s) { server.sendContent(s); }
static void ws(const String &s) { server.sendContent(s); }
static void wsEnd() { server.sendContent(""); }



static int batData = 0;
static bool usbPlugged = false;

extern TinyGPSPlus gps;

#ifdef USE_PMU
#include <XPowersLib.h>
extern XPowersPMU PMU;
extern bool vbusIn;
#endif

static String buildBatteryRows() {
    String rows = "";
#ifdef USE_PMU
    bool batPresent = PMU.isBatteryConnect();
    const char *state;
    if (!batPresent) {
        state = "No Battery";
    } else {
        switch (PMU.getChargerStatus()) {
            case XPOWERS_AXP2101_CHG_TRI_STATE:  state = "Trickle chg"; break;
            case XPOWERS_AXP2101_CHG_PRE_STATE:  state = "Pre-charge";  break;
            case XPOWERS_AXP2101_CHG_CC_STATE:   state = "Charging CC"; break;
            case XPOWERS_AXP2101_CHG_CV_STATE:   state = "Charging CV"; break;
            case XPOWERS_AXP2101_CHG_DONE_STATE: state = "Full";        break;
            default: state = vbusIn ? "Standby" : "Discharging";        break;
        }
    }
    rows += "<tr><td>State</td><td align=\"right\">" + String(state) + "</td></tr>";
    rows += "<tr><td>Battery</td><td align=\"right\">" +
            (batPresent ? String((int)PMU.getBattVoltage()) + " mV / " + String((int)PMU.getBatteryPercent()) + "%" : "absent") +
            "</td></tr>";
    rows += "<tr><td>VBUS</td><td align=\"right\">" +
            (vbusIn ? String((int)PMU.getVbusVoltage()) + " mV" : "&#8212;") +
            "</td></tr>";
    rows += "<tr><td>Sys Voltage</td><td align=\"right\">" + String((int)PMU.getSystemVoltage()) + " mV</td></tr>";
    rows += "<tr><td>PMU Temp</td><td align=\"right\">" + String(PMU.getTemperature(), 1) + " &deg;C</td></tr>";
#else
    String batStr = batData > 0 ? String(batData) + "%" : "N/A";
    rows += "<tr><td>Battery</td><td align=\"right\">" + batStr + "</td></tr>";
    if (usbPlugged) rows += "<tr><td>USB</td><td align=\"right\">Plugged</td></tr>";
#endif
    return rows;
}

void WebDataUpdate(int _batData, bool _usbPlugged) {
    batData = _batData;
    usbPlugged = _usbPlugged;
}

void serviceHandle() {
    dnsServer.processNextRequest();
    server.handleClient();
}
using TplVars = std::vector<std::pair<String, String>>;
static void streamTemplate(const char *path, const TplVars &vars);

void setHTML(byte page) {
    wsBegin();

    String autoRefresh = "";

    String pageScripts = "";
    if (page == 0) {
        pageScripts = "<script src=\"/jquery.min.js\"></script>\n"
                      "<script src=\"/dashboard.js\"></script>\n";
    } else if (page == 6) {
        pageScripts = "<script src=\"/jquery.min.js\"></script>\n"
                      "<script src=\"/highcharts.js\"></script>\n"
                      "<script src=\"/highcharts-more.js\"></script>\n"
                      "<script src=\"/vu.js\"></script>\n";
    } else if (page == 7) {
        pageScripts = "<script src=\"/radio.js\"></script>\n";
    }

    String myStation;
    if (config.aprs_ssid == 0)
        myStation = String(config.aprs_mycall);
    else
        myStation = String(config.aprs_mycall) + "-" + String(config.aprs_ssid);

    String storageTab = "";
#ifdef SDCARD
    storageTab = "<a href=\"/data\" class=\"nav-link " + String(page == 1 ? "active" : "") + "\">Storage</a>";
#endif

    if (page == 9) {
        pageScripts = "<link rel=\"stylesheet\" href=\"/codemirror.min.css\">\n"
                      "<link rel=\"stylesheet\" href=\"/cm-theme-dracula.min.css\">\n"
                      "<script src=\"/jquery.min.js\"></script>\n"
                      "<script src=\"/codemirror.min.js\"></script>\n"
                      "<script src=\"/cm-mode-js.min.js\"></script>\n"
                      "<script src=\"/cm-mode-css.min.js\"></script>\n"
                      "<script src=\"/cm-mode-xml.min.js\"></script>\n"
                      "<script src=\"/cm-mode-html.min.js\"></script>\n"
                      "<script src=\"/files.js\"></script>\n";
    }

    streamTemplate("/header.html", {
        {"auto_refresh",     autoRefresh},
        {"page_scripts",     pageScripts},
        {"my_station",       myStation},
        {"version",          String(VERSION_FULL)},
        {"nav_storage_tab",  storageTab},
        {"nav_dash",         page == 0 ? String("active") : String("")},
        {"nav_setting",      page == 2 ? String("active") : String("")},
        {"nav_radio",        page == 7 ? String("active") : String("")},
        {"nav_service",      page == 3 ? String("active") : String("")},
        {"nav_system",       page == 4 ? String("active") : String("")},
        {"nav_test",         page == 6 ? String("active") : String("")},
        {"nav_firmware",     page == 5 ? String("active") : String("")},
        {"nav_config",       page == 8 ? String("active") : String("")},
        {"nav_files",        page == 9 ? String("active") : String("")}
    });
}

void handle_root() {
    setHTML(0);

    char strTime[30];
    char uptimeBuf[30];
    struct tm tmstruct;
    tmstruct.tm_year = 0;
    getLocalTime(&tmstruct, 0);
    sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d",
            (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1,
            tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

    uint64_t tn = esp_timer_get_time() / 1000000ULL;
    sprintf(uptimeBuf, "%llu days %02d:%02d:%02d", tn / 86400,
            (int)(tn % 86400) / 3600, (int)(tn % 3600) / 60, (int)(tn % 60));

    bool gpsValid = gps.location.isValid() && gps.altitude.isValid() &&
                    gps.speed.isValid() && gps.course.isValid() &&
                    gps.satellites.isValid() && gps.hdop.isValid();
    bool gpsOnline = GpsPktCnt() > 0;

    String gnssRows = "";
    if (gpsOnline) {
        const char *fixStatus = gpsValid ? "FIX OK" : "Waiting for FIX";
        gnssRows += "<tr><td>Status</td><td align=\"right\">" + String(fixStatus) +
                    (gpsValid ? ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Age " + String(gps.location.age() / 1000) + " s") : "") + "</td></tr>";
        gnssRows += "<tr><td>Coords</td><td align=\"right\">" +
                    (gpsValid ? (String(gps.location.lat(), 6) + "&deg;" + (gps.location.rawLat().negative ? "S" : "N") +
                                 " / " + String(gps.location.lng(), 6) + "&deg;" + (gps.location.rawLng().negative ? "W" : "E")) : "-") + "</td></tr>";
        gnssRows += "<tr><td>Altitude</td><td align=\"right\">" + (gpsValid ? String(gps.altitude.meters(), 1) + " m" : "-") + "</td></tr>";
        gnssRows += "<tr><td>Speed / Course</td><td align=\"right\">" +
                    (gpsValid ? String(gps.speed.kmph(), 1) + " km/h / " + String(gps.course.deg(), 1) + "&deg;" : "-") + "</td></tr>";
        gnssRows += "<tr><td>Sats / HDOP</td><td align=\"right\">" +
                    String(gps.satellites.value()) + " / " + (gpsValid ? String(gps.hdop.hdop(), 1) : "-") + "</td></tr>";
        gnssRows += "<tr><td>GPS / BDS / GLO</td><td align=\"right\">" +
                    String(gnssSatsGPS) + " / " + String(gnssSatsBDS) + " / " + String(gnssSatsGLO) + "</td></tr>";
    } else {
        gnssRows = "<tr><td>Status</td><td align=\"right\">NO GNSS DATA</td></tr>";
    }

    String lastStationsRows = "";
    sort(pkgList, PKGLISTSIZE);
    for (int i = 0; i < PKGLISTSIZE && i <= 20; i++) {
        if (pkgList[i].time > 0) {
            pkgList[i].calsign[10] = 0;
            localtime_r(&pkgList[i].time, &tmstruct);
            sprintf(strTime, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
            lastStationsRows += "<tr><td align=\"left\">" + String(strTime) +
                "</td><td align=\"center\">" + String(pkgList[i].calsign) +
                "</td><td align=\"center\">" + (pkgList[i].channel == 0 ? "RF" : "Net") +
                "</td><td align=\"right\">" + pkgGetType(pkgList[i].type) + "</td></tr>";
        }
    }

    String topSendRows = "";
    sortPkgDesc(pkgList, PKGLISTSIZE);
    for (int i = 0; i < PKGLISTSIZE && i <= 20; i++) {
        if (pkgList[i].time > 0) {
            pkgList[i].calsign[10] = 0;
            topSendRows += "<tr><td align=\"left\">" + String(pkgList[i].calsign) +
                           "</td><td align=\"right\">" + String(pkgList[i].pkg, DEC) + "</td></tr>";
        }
    }

    String cpuTemp   = String(temperatureRead(), 1);
    String freeHeap  = String(ESP.getFreeHeap());
    String statsAll  = String(status.allCount) + " / " + String(status.dropCount);
    String statsRf   = String(status.rf2inet)  + " / " + String(status.inet2rf);

    streamTemplate("/dashboard.html", {
        {"last_readings_time", String(strTime)},
        {"cpu_temp",           cpuTemp},
        {"free_heap",          freeHeap},
        {"uptime",             String(uptimeBuf)},
        {"wifi_rssi",          String(WiFi.RSSI())},
        {"battery_rows",       buildBatteryRows()},
        {"gnss_rows",          gnssRows},
        {"stats_all",          statsAll},
        {"stats_rf",           statsRf},
        {"last_stations_rows", lastStationsRows},
        {"top_send_rows",      topSendRows}
    });
    wsEnd();
}

#ifdef SDCARD
void handle_storage() {
    String dirname = "/";
    char strTime[100];

    if (server.args() > 0) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "SD_INIT") {
                // SD.end();
                // if (!SD.begin(SDCARD_CS, spiSD, SDSPEED)) {
                //	Serial.println("SD CARD Initialization failed!");
                //	//return;
                // }
            }
        }
    }

    setHTML(1);

    uint8_t cardType = SD.cardType();
    String cardTypeStr = "UNKNOWN";
    if      (cardType == CARD_NONE) cardTypeStr = "NOT FOUND";
    else if (cardType == CARD_MMC)  cardTypeStr = "MMC";
    else if (cardType == CARD_SD)   cardTypeStr = "SDSC";
    else if (cardType == CARD_SDHC) cardTypeStr = "SDHC";

    String cardStats = "";
    String fileSection = "";
    if (cardType != CARD_NONE) {
        unsigned long cardSize  = SD.cardSize()   / (1024 * 1024);
        unsigned long cardTotal = SD.totalBytes() / (1024 * 1024);
        unsigned long cardUsed  = SD.usedBytes()  / (1024 * 1024);
        cardStats = "<br><b>SD Card Size:</b> " + String((double)cardSize / 1000, 1) + "GB<br>\n"
                    "<b>Total space:</b> " + String(cardTotal) + "MB<br>\n"
                    "<b>Used space:</b> "  + String(cardUsed)  + "MB<br>\n"
                    "<br>Listing directory: " + dirname + "<br>\n";

        File root = SD.open(dirname);
        if (!root || !root.isDirectory()) {
            fileSection = root ? "<p>Not a directory</p>" : "<p>Failed to open directory</p>";
        } else {
            fileSection = "<table border=\"1\"><tr align=\"center\" bgcolor=\"#03DDFC\">"
                          "<td><b>DIRECTORY</b></td><td width=\"150\"><b>FILE NAME</b></td>"
                          "<td width=\"100\"><b>SIZE(Byte)</b></td>"
                          "<td width=\"170\"><b>DATE TIME</b></td><td><b>DEL</b></td></tr>";
            char fmtBuf[80];
            File file = root.openNextFile();
            while (file) {
                if (file.isDirectory()) {
                    time_t t = file.getLastWrite();
                    struct tm *ts = localtime(&t);
                    sprintf(fmtBuf, "<td></td><td></td><td align=\"right\">%d-%02d-%02d %02d:%02d:%02d</td>",
                            (ts->tm_year)+1900, (ts->tm_mon)+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
                    fileSection += "<tr><td>" + String(file.name()) + "</td>" + String(fmtBuf) + "<td></td></tr>\n";
                } else {
                    String fName = String(file.name()).substring(1);
                    time_t t = file.getLastWrite();
                    struct tm *ts = localtime(&t);
                    sprintf(fmtBuf, "<td align=\"right\">%d-%02d-%02d %02d:%02d:%02d</td>",
                            (ts->tm_year)+1900, (ts->tm_mon)+1, ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec);
                    fileSection += "<tr><td>/</td><td align=\"right\"><a href=\"/download?FILE=" + fName +
                                   "\" target=\"_blank\">" + fName + "</a></td>"
                                   "<td align=\"right\">" + String(file.size()) + "</td>" +
                                   String(fmtBuf) +
                                   "<td align=\"center\"><a href=\"/delete?FILE=" + fName + "\">X</a></td></tr>\n";
                }
                file = root.openNextFile();
            }
            fileSection += "</table>";
        }
    }

    streamTemplate("/storage.html", {
        {"card_type",    cardTypeStr},
        {"card_stats",   cardStats},
        {"file_section", fileSection}
    });
    wsEnd();
}

#endif

static void streamTemplate(const char *path, const TplVars &vars) {
    File f = LittleFS.open(path, "r");
    if (!f) { ws("[template missing: "); ws(path); ws("]"); return; }
    String content = f.readString();
    f.close();
    for (auto &kv : vars) content.replace("{" + kv.first + "}", kv.second);
    ws(content);
}

void handle_setting() {
    // byte *ptr;
    bool synctime = false;
    //	bool taretime = false;
    // bool davisEn = false;
    // bool moniEn = false;

    {
#ifndef I2S_INTERNAL
        AFSK_TimerEnable(false);
#endif
        if (server.args() > 0) {
            synctime = false;
            //			taretime = false;
            for (uint8_t i = 0; i < server.args(); i++) {
                // Serial.print("SERVER ARGS ");
                // Serial.print(server.argName(i));
                // Serial.print("=");
                // Serial.println(server.arg(i));

                if (server.argName(i) == "synctime") {
                    if (server.arg(i) != "") {
                        // if (isValidNumber(server.arg(i)))
                        if (String(server.arg(i)) == "OK") synctime = true;
                    }
                }

                // if (server.argName(i) == "taretime") {
                //	if (server.arg(i) != "")
                //	{
                //		//if (isValidNumber(server.arg(i)))
                //		if (String(server.arg(i)) == "OK")
                //			taretime = true;
                //	}
                // }
                if (server.argName(i) == "gpsLat") {
                    if (server.arg(i) != "") {
                        config.gps_lat = server.arg(i).toFloat();
                    }
                }
                if (server.argName(i) == "gpsLon") {
                    if (server.arg(i) != "") {
                        config.gps_lon = server.arg(i).toFloat();
                    }
                }
                if (server.argName(i) == "gpsAlt") {
                    if (server.arg(i) != "") {
                        config.gps_alt = server.arg(i).toFloat();
                    }
                }
                if (server.argName(i) == "gps_mode") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.gps_mode = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "moniCall") {
                    if (server.arg(i) != "") {
                        strcpy(config.aprs_moniCall, server.arg(i).c_str());
                    }
                }

                if (server.argName(i) == "iconTable") {
                    if (server.arg(i) != "") {
                        config.aprs_table = *server.arg(i).c_str();
                    }
                }
                if (server.argName(i) == "iconSymbol") {
                    if (server.arg(i) != "") {
                        config.aprs_symbol = *server.arg(i).c_str();
                    }
                }

                if (server.argName(i) == "aprsPath") {
                    if (server.arg(i) != "") {
                        strcpy(config.aprs_path, server.arg(i).c_str());
                    }
                }

                if (server.argName(i) == "beaconIntv") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.aprs_beacon = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "comment") {
                    if (server.arg(i) != "") {
                        strcpy(config.aprs_comment, server.arg(i).c_str());
                    }
                }

                if (server.argName(i) == "sbFastSpeed") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_fast_speed = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "sbFastRate") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_fast_rate = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "sbSlowSpeed") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_slow_speed = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "sbSlowRate") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_slow_rate = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "sbMinTurnTime") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_turn_time = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "sbMinTurnAngle") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_turn_min = server.arg(i).toInt();
                    }
                }
                if (server.argName(i) == "sbTurnSlope") {
                    if (server.arg(i) != "") {
                        if (isValidNumber(server.arg(i)))
                            config.sb_turn_slope = server.arg(i).toInt();
                    }
                }
                
            }

            config.synctime = synctime;
            SaveConfig();
            // topBar(WiFi.RSSI());
        }
#ifndef I2S_INTERNAL
        AFSK_TimerEnable(true);
#endif
    }

    String gpsModeOpts = "";
    for (int i = 0; i < 3; i++) {
        gpsModeOpts += "<option value=\"" + String(i) + "\"";
        if (config.gps_mode == i) gpsModeOpts += " selected";
        gpsModeOpts += ">" + String(gpsMode[i]) + "</option>";
    }

    setHTML(2);
    streamTemplate("/setting.html", {
        {"gps_lat",          String(config.gps_lat, 5)},
        {"gps_lon",          String(config.gps_lon, 5)},
        {"gps_alt",          String(config.gps_alt)},
        {"gps_mode_options", gpsModeOpts},
        {"aprs_table",       String(config.aprs_table)},
        {"aprs_symbol",      String(config.aprs_symbol)},
        {"aprs_path",        String(config.aprs_path)},
        {"aprs_beacon",      String(config.aprs_beacon)},
        {"comment_maxlen",   String(sizeof(config.aprs_comment))},
        {"aprs_comment",     String(config.aprs_comment)},
        {"synctime_checked", config.synctime ? String("checked") : String("")},
        {"sb_fast_speed",    String(config.sb_fast_speed)},
        {"sb_fast_rate",     String(config.sb_fast_rate)},
        {"sb_slow_speed",    String(config.sb_slow_speed)},
        {"sb_slow_rate",     String(config.sb_slow_rate)},
        {"sb_turn_time",     String(config.sb_turn_time)},
        {"sb_turn_min",      String(config.sb_turn_min)},
        {"sb_turn_slope",    String(config.sb_turn_slope)}
    });
    wsEnd();
}

void handle_service() {
    bool aprsEn = false;
    bool tncEn = false;
    bool digiEn = false;
    bool tlmEn = false;
    bool rf2inetEn = false;
    bool inet2rfEn = false;
    bool hpfEn = false;

    if (server.hasArg("commit")) {
#ifndef I2S_INTERNAL
        AFSK_TimerEnable(false);
#endif
        for (uint8_t i = 0; i < server.args(); i++) {
            // Serial.print("SERVER ARGS ");
            // Serial.print(server.argName(i));
            // Serial.print("=");
            // Serial.println(server.arg(i));
            if (server.argName(i) == "aprsEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") aprsEn = true;
                }
            }
            if (server.argName(i) == "myCall") {
                if (server.arg(i) != "") {
                    String str = server.arg(i);
                    str.trim();
                    str.toUpperCase();
                    int dash = str.indexOf('-');
                    if (dash > 0) str = str.substring(0, dash);
                    strcpy(config.aprs_mycall, str.c_str());
                }
            }
            if (server.argName(i) == "myobject") {
                if (server.arg(i) != "") {
                    String str = server.arg(i);
                    str.trim();
                    strcpy(config.aprs_object, str.c_str());
                    // for (int i = strlen(config.aprs_object); i < 9; i++) {
                    // config.aprs_object[i] = 0x20; } config.aprs_object[9] =
                    // 0;
                } else {
                    config.aprs_object[0] = 0;
                }
            }
            if (server.argName(i) == "mySSID") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.aprs_ssid = server.arg(i).toInt();
                    if (config.aprs_ssid > 15) config.aprs_ssid = 15;
                }
            }
            if (server.argName(i) == "myPasscode") {
                if (server.arg(i) != "") {
                    strcpy(config.aprs_passcode, server.arg(i).c_str());
                }
            }
            if (server.argName(i) == "aprsHost") {
                if (server.arg(i) != "") {
                    strcpy(config.aprs_host, server.arg(i).c_str());
                }
            }
            if (server.argName(i) == "aprsPort") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.aprs_port = server.arg(i).toInt();
                }
            }
            if (server.argName(i) == "aprsFilter") {
                if (server.arg(i) != "") {
                    strcpy(config.aprs_filter, server.arg(i).c_str());
                }
            }

            // if (server.argName(i) == "aprsComment") {
            // 	if (server.arg(i) != "")
            // 	{
            // 		strcpy(config.tnc_comment, server.arg(i).c_str());
            // 	}
            // }
            if (server.argName(i) == "tncEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") tncEn = true;
                }
            }
            if (server.argName(i) == "digiEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") digiEn = true;
                }
            }
            if (server.argName(i) == "tlmEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") tlmEn = true;
                }
            }
            if (server.argName(i) == "rf2inetEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") rf2inetEn = true;
                }
            }
            if (server.argName(i) == "inet2rfEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") inet2rfEn = true;
                }
            }
            if (server.argName(i) == "hpfEnable") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") hpfEn = true;
                }
            }
            if (server.argName(i) == "digiDelay") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.digi_delay = server.arg(i).toInt();
                    if (config.digi_delay > 5000) config.digi_delay = 5000;
                }
            }
            if (server.argName(i) == "timeSlot") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.tx_timeslot = server.arg(i).toInt();
                    if (config.tx_timeslot > 10000) config.tx_timeslot = 10000;
                }
            }
        }
        config.aprs = aprsEn;
        config.tnc = tncEn;
        config.tnc_digi = digiEn;
        config.tnc_telemetry = tlmEn;
        config.rf2inet = rf2inetEn;
        config.inet2rf = inet2rfEn;
        config.input_hpf = hpfEn;
        input_HPF = hpfEn;
        SaveConfig();
// if(config.tnc) tncInit();
#ifndef I2S_INTERNAL
        AFSK_TimerEnable(true);
#endif
    }

    String ssidOpts = "";
    for (uint8_t ssid = 0; ssid <= 15; ssid++) {
        ssidOpts += "<option value=\"" + String(ssid) + "\"";
        if (config.aprs_ssid == ssid) ssidOpts += " selected";
        ssidOpts += ">" + String(ssid) + "</option>";
    }

    String digiDelayOpts = "";
    for (uint16_t d = 0; d <= 5000; d += 500) {
        digiDelayOpts += "<option value=\"" + String(d) + "\"";
        if (config.digi_delay == d) digiDelayOpts += " selected";
        digiDelayOpts += ">" + (d == 0 ? String("Auto") : String(d)) + "</option>";
    }

    String timeslotOpts = "";
    for (uint16_t d = 0; d <= 10000; d += 1000) {
        timeslotOpts += "<option value=\"" + String(d) + "\"";
        if (config.tx_timeslot == d) timeslotOpts += " selected";
        timeslotOpts += ">" + String(d) + "</option>";
    }

    setHTML(3);
    streamTemplate("/service.html", {
        {"aprs_enable_checked", config.aprs ? String("checked") : String("")},
        {"aprs_mycall",         String(config.aprs_mycall)},
        {"aprs_ssid_options",   ssidOpts},
        {"aprs_passcode",       String(config.aprs_passcode)},
        {"aprs_object",         String(config.aprs_object)},
        {"aprs_host",           String(config.aprs_host)},
        {"aprs_port",           String(config.aprs_port)},
        {"aprs_filter",         String(config.aprs_filter)},
        {"tnc_checked",         config.tnc ? String("checked") : String("")},
        {"tlm_checked",         config.tnc_telemetry ? String("checked") : String("")},
        {"rf2inet_checked",     config.rf2inet ? String("checked") : String("")},
        {"inet2rf_checked",     config.inet2rf ? String("checked") : String("")},
        {"hpf_checked",         config.input_hpf ? String("checked") : String("")},
        {"digi_checked",        config.tnc_digi ? String("checked") : String("")},
        {"digi_delay_options",  digiDelayOpts},
        {"timeslot_options",    timeslotOpts}
    });
    wsEnd();
}

void handle_radio() {
    // bool noiseEn=false;
    // bool agcEn=false;
    if (server.hasArg("commit")) {
        for (uint8_t i = 0; i < server.args(); i++) {
            // Serial.print("SERVER ARGS ");
            // Serial.print(server.argName(i));
            // Serial.print("=");
            // Serial.println(server.arg(i));

            // if (server.argName(i) == "agcCheck")
            // {
            // 	if (server.arg(i) != "")
            // 		{
            // 			if (String(server.arg(i)) == "OK")
            // 			{
            // 				agcEn=true;
            // 			}
            // 		}
            // }

            if (server.argName(i) == "nw_band") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        config.band = server.arg(i).toInt();
                        // if (server.arg(i).toInt())
                        // 	config.band = 1;
                        // else
                        // 	config.band = 0;
                    }
                }
            }

            if (server.argName(i) == "volume") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.volume = server.arg(i).toInt();
                }
            }

            if (server.argName(i) == "rf_power") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        if (server.arg(i).toInt())
                            config.rf_power = true;
                        else
                            config.rf_power = false;
                    }
                }
            }

            if (server.argName(i) == "oled_brightness") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        uint8_t idx = (uint8_t)constrain(server.arg(i).toInt(), 0, BRIGHTNESS_LEVEL_COUNT - 1);
                        config.oled_brightness = BRIGHTNESS_LEVELS[idx];
                        OledSetBrightness(config.oled_brightness);
                    }
                }
            }

            if (server.argName(i) == "locator_popup") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.locator_popup = (uint8_t)constrain(server.arg(i).toInt(), 0, 10);
                }
            }

            if (server.argName(i) == "oled_autodim") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.oled_autodim = (uint8_t)constrain(server.arg(i).toInt(), 0, 60);
                }
            }

            if (server.argName(i) == "oled_autodim_level") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.oled_autodim_level = (uint8_t)constrain(server.arg(i).toInt(), 0, 4);
                }
            }

            if (server.argName(i) == "aprs_rx_popup") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.aprs_rx_popup = (uint8_t)constrain(server.arg(i).toInt(), 0, 10);
                }
            }

            if (server.argName(i) == "aprs_sms_popup") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        int v = server.arg(i).toInt();
                        if (v == 0 || v == 15 || v == 30 || v == 60)
                            config.aprs_sms_popup = (uint8_t)v;
                    }
                }
            }

            if (server.argName(i) == "sql_level") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.sql_level = server.arg(i).toInt();
                }
            }

            if (server.argName(i) == "tx_freq") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.freq_tx = server.arg(i).toFloat();
                }
            }
            if (server.argName(i) == "rx_freq") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.freq_rx = server.arg(i).toFloat();
                }
            }

            if (server.argName(i) == "tx_offset") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.offset_tx = server.arg(i).toInt();
                }
            }
            if (server.argName(i) == "rx_offset") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.offset_rx = server.arg(i).toInt();
                }
            }

            if (server.argName(i) == "tx_ctcss") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.tone_tx = server.arg(i).toInt();
                }
            }
            if (server.argName(i) == "rx_ctcss") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.tone_rx = server.arg(i).toInt();
                }
            }

            if (server.argName(i) == "rx_att") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        if (server.arg(i).toInt())
                            config.rx_att = true;
                        else
                            config.rx_att = false;
                    }
                }
            }
        }
        // config.noise=noiseEn;
        // config.agc=agcEn;
        SaveConfig();
        // delay(100);
        RF_Init(false);
    }

#ifndef BAND_70CM
    const char *txFreqMin = "144.0000", *txFreqMax = "148.0000";
    const char *rxFreqMin = "144.0000", *rxFreqMax = "148.0000";
#else
    const char *txFreqMin = "430.0000", *txFreqMax = "440.0000";
    const char *rxFreqMin = "432.0000", *rxFreqMax = "433.0000";
#endif

    String rxAttSection = "";
#if defined(BOARD_TTWR_PLUS)
    String rxAttSelT = config.rx_att ? String("selected") : String("");
    String rxAttSelF = config.rx_att ? String("") : String("selected");
    rxAttSection =
        "<div class=\"form-group\">\n"
        "<label class=\"col-sm-3 col-xs-12 control-label\">RX ADC Attenuation</label>\n"
        "<div class=\"col-sm-9 col-xs-12\">"
        "<div class=\"col-sm-3 col-xs-6\"><select name=\"rx_att\" id=\"rx_att\">"
        "<option value=\"0\" " + rxAttSelF + ">11dB (stock)</option>"
        "<option value=\"1\" " + rxAttSelT + ">2.5dB (R22 mod)</option>"
        "</select></div>\n"
        "<span class=\"help-block col-sm-9 col-xs-12\" style=\"margin-top:0\">"
        "<b>11dB</b>: stock hardware, R22=4.7K&Omega;, DC bias ~2.2V. "
        "<b>2.5dB</b>: requires R22 replaced 4.7K&rarr;39K&Omega;, DC bias ~0.68V &mdash; 2.4&times; better audio resolution. "
        "Wrong choice for your hardware = no decodes."
        "</span>"
        "</div>\n</div>\n";
#endif

    setHTML(7);
    streamTemplate("/radio.html", {
        {"tx_freq_min",       String(txFreqMin)},
        {"tx_freq_max",       String(txFreqMax)},
        {"tx_freq",           String(config.freq_tx, 4)},
        {"rx_freq_min",       String(rxFreqMin)},
        {"rx_freq_max",       String(rxFreqMax)},
        {"rx_freq",           String(config.freq_rx, 4)},
        {"nw_wide_sel",       config.band ? String("selected") : String("")},
        {"nw_narrow_sel",     config.band ? String("") : String("selected")},
        {"rf_power_high_sel", config.rf_power ? String("selected") : String("")},
        {"rf_power_low_sel",  config.rf_power ? String("") : String("selected")},
        {"rx_att_section",    rxAttSection},
        {"volume",            String(config.volume)},
        {"sql_level",         String(config.sql_level)},
        {"oled_brightness_idx",  String(brightnessIdx(config.oled_brightness))},
        {"oled_brightness_name", String(BRIGHTNESS_NAMES[brightnessIdx(config.oled_brightness)])},
        {"autodim_off",   config.oled_autodim == 0  ? String("selected") : String("")},
        {"autodim_5",     config.oled_autodim == 5  ? String("selected") : String("")},
        {"autodim_10",    config.oled_autodim == 10 ? String("selected") : String("")},
        {"autodim_20",    config.oled_autodim == 20 ? String("selected") : String("")},
        {"autodim_30",    config.oled_autodim == 30 ? String("selected") : String("")},
        {"autodim_60",    config.oled_autodim == 60 ? String("selected") : String("")},
        {"autodim_lvl_0", config.oled_autodim_level == 0 ? String("selected") : String("")},
        {"autodim_lvl_1", config.oled_autodim_level == 1 ? String("selected") : String("")},
        {"autodim_lvl_2", config.oled_autodim_level == 2 ? String("selected") : String("")},
        {"autodim_lvl_3", config.oled_autodim_level == 3 ? String("selected") : String("")},
        {"autodim_lvl_4", config.oled_autodim_level == 4 ? String("selected") : String("")},
        {"sms_popup_off", config.aprs_sms_popup == 0  ? String("selected") : String("")},
        {"sms_popup_15",  config.aprs_sms_popup == 15 ? String("selected") : String("")},
        {"sms_popup_30",  config.aprs_sms_popup == 30 ? String("selected") : String("")},
        {"sms_popup_60",  config.aprs_sms_popup == 60 ? String("selected") : String("")},
        {"rx_popup_off",  config.aprs_rx_popup == 0  ? String("selected") : String("")},
        {"rx_popup_1",    config.aprs_rx_popup == 1  ? String("selected") : String("")},
        {"rx_popup_2",    config.aprs_rx_popup == 2  ? String("selected") : String("")},
        {"rx_popup_3",    config.aprs_rx_popup == 3  ? String("selected") : String("")},
        {"rx_popup_4",    config.aprs_rx_popup == 4  ? String("selected") : String("")},
        {"rx_popup_5",    config.aprs_rx_popup == 5  ? String("selected") : String("")},
        {"rx_popup_6",    config.aprs_rx_popup == 6  ? String("selected") : String("")},
        {"rx_popup_7",    config.aprs_rx_popup == 7  ? String("selected") : String("")},
        {"rx_popup_8",    config.aprs_rx_popup == 8  ? String("selected") : String("")},
        {"rx_popup_9",    config.aprs_rx_popup == 9  ? String("selected") : String("")},
        {"rx_popup_10",   config.aprs_rx_popup == 10 ? String("selected") : String("")},
        {"locator_popup_off", config.locator_popup == 0  ? String("selected") : String("")},
        {"locator_popup_1",   config.locator_popup == 1  ? String("selected") : String("")},
        {"locator_popup_2",   config.locator_popup == 2  ? String("selected") : String("")},
        {"locator_popup_3",   config.locator_popup == 3  ? String("selected") : String("")},
        {"locator_popup_4",   config.locator_popup == 4  ? String("selected") : String("")},
        {"locator_popup_5",   config.locator_popup == 5  ? String("selected") : String("")},
        {"locator_popup_6",   config.locator_popup == 6  ? String("selected") : String("")},
        {"locator_popup_7",   config.locator_popup == 7  ? String("selected") : String("")},
        {"locator_popup_8",   config.locator_popup == 8  ? String("selected") : String("")},
        {"locator_popup_9",   config.locator_popup == 9  ? String("selected") : String("")},
        {"locator_popup_10",  config.locator_popup == 10 ? String("selected") : String("")}
    });
    wsEnd();
}


void handle_system() {
    if (server.hasArg("updateTimeZone")) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "SetTimeZone") {
                if (server.arg(i) != "") {
                    config.timeZone = server.arg(i).toInt();
                    configTime(3600 * config.timeZone, 0, config.ntpServer);
                }
                break;
            }
        }
        SaveConfig();
    } else if (server.hasArg("updateTimeNtp")) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "SetTimeNtp") {
                if (server.arg(i) != "") {
                    log_i("WEB Config NTP");
                    strcpy(config.ntpServer, server.arg(i).c_str());
                    configTime(3600 * config.timeZone, 0, config.ntpServer);
                }
                break;
            }
        }
        SaveConfig();
    } else if (server.hasArg("updateTime")) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "SetTime") {
                if (server.arg(i) != "") {
                    String raw = server.arg(i);
                    raw.replace("T", " ");
                    String date = getValue(raw, ' ', 0);
                    String time = getValue(raw, ' ', 1);
                    int yyyy = getValue(date, '-', 0).toInt();
                    int mm = getValue(date, '-', 1).toInt();
                    int dd = getValue(date, '-', 2).toInt();
                    int hh = getValue(time, ':', 0).toInt();
                    int ii = getValue(time, ':', 1).toInt();
                    int ss = getValue(time, ':', 2).toInt();

                    tmElements_t timeinfo;
                    timeinfo.Year = yyyy - 1970;
                    timeinfo.Month = mm;
                    timeinfo.Day = dd;
                    timeinfo.Hour = hh;
                    timeinfo.Minute = ii;
                    timeinfo.Second = ss;
                    time_t timeStamp = makeTime(timeinfo);

                    time_t rtc = timeStamp - (config.timeZone * 3600);
                    timeval tv = {rtc, 0};
                    timezone tz = {(0) + DST_MN, 0};
                    settimeofday(&tv, &tz);

                    log_i("Set New Time at %d/%d/%d %d:%d:%d %d", dd, mm, yyyy, hh, ii, ss, timeStamp);
                }
                break;
            }
        }
        SaveConfig();
    } else if (server.hasArg("updateWifi")) {
        uint8_t oldWifiMode = config.wifi_mode;
        bool wifiSTA = false;
        bool wifiAP = false;
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "wifiAP") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") {
                        wifiAP = true;
                    }
                }
            }
            if (server.argName(i) == "wificlient") {
                if (server.arg(i) != "") {
                    if (String(server.arg(i)) == "OK") {
                        wifiSTA = true;
                    }
                }
            }

            if (server.argName(i) == "gpsLat") {
                if (server.arg(i) != "") {
                    config.gps_lat = server.arg(i).toFloat();
                }
            }
            if (server.argName(i) == "gpsLon") {
                if (server.arg(i) != "") {
                    config.gps_lon = server.arg(i).toFloat();
                }
            }
            if (server.argName(i) == "gpsAlt") {
                if (server.arg(i) != "") {
                    config.gps_alt = server.arg(i).toFloat();
                }
            }

            if (server.argName(i) == "wifi_ssidAP") {
                if (server.arg(i) != "") {
                    strcpy(config.wifi_ap_ssid, server.arg(i).c_str());
                }
            }
            if (server.argName(i) == "wifi_passAP") {
                if (server.arg(i) != "") {
                    strcpy(config.wifi_ap_pass, server.arg(i).c_str());
                }
            }
            if (server.argName(i) == "wifi_ssid") {
                if (server.arg(i) != "") {
                    strcpy(config.wifi_ssid, server.arg(i).c_str());
                }
            }
            if (server.argName(i) == "wifi_pass") {
                if (server.arg(i) != "") {
                    strcpy(config.wifi_pass, server.arg(i).c_str());
                }
            }
            if (server.argName(i) == "wifi_pwr") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i)))
                        config.wifi_power = server.arg(i).toInt();
                }
            }
        }
        if (wifiAP && wifiSTA) {
            config.wifi_mode = WIFI_AP_STA_FIX;
        } else if (wifiAP) {
            config.wifi_mode = WIFI_AP_FIX;
        } else if (wifiSTA) {
            config.wifi_mode = WIFI_STA_FIX;
        } else {
            config.wifi_mode = WIFI_OFF_FIX;
        }
        SaveConfig();
        if (config.wifi_mode != oldWifiMode) {
            server.send(200, "text/html",
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<link rel='stylesheet' href='/style.css'>"
                "<script>var t=5;setInterval(function(){"
                "var e=document.getElementById('c');if(e)e.textContent=--t;"
                "if(t<=0)location.replace('/system');"
                "},1000);</script></head><body>"
                "<div class='topnav'><span>APRS-ESP32</span></div>"
                "<div class='page-content' style='padding:40px;text-align:center'>"
                "<h3>WiFi settings saved</h3>"
                "<p style='margin-top:16px'>Device is rebooting. "
                "Page will reload in <b id='c'>5</b> seconds...</p>"
                "</div></body></html>");
            delay(1500);
            esp_restart();
        }
    } else if (server.hasArg("REBOOT")) {
        server.send(200, "text/html", "Rebooting...");
        delay(1000);
        esp_restart();
    }

    struct tm tmstruct;
    char strTime[20], strTimeISO[20];
    tmstruct.tm_year = 0;
    getLocalTime(&tmstruct, 0);
    sprintf(strTime,    "%d-%02d-%02d %02d:%02d:%02d",
            (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday,
            tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    sprintf(strTimeISO, "%d-%02d-%02dT%02d:%02d:%02d",
            (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday,
            tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

    String wifiPwrOpts = "";
    for (int i = 0; i < 12; i++) {
        wifiPwrOpts += "<option value=\"" + String(wifiPwr[i][0], 0) + "\"";
        if (config.wifi_power == wifiPwr[i][0]) wifiPwrOpts += " selected";
        wifiPwrOpts += ">" + String(wifiPwr[i][1], 1) + " dBm</option>";
    }

    String wifiMode = "OFF";
    if (config.wifi_mode == WIFI_AP_FIX)          wifiMode = "AP";
    else if (config.wifi_mode == WIFI_STA_FIX)    wifiMode = "STA";
    else if (config.wifi_mode == WIFI_AP_STA_FIX) wifiMode = "AP+STA";

    setHTML(4);
    streamTemplate("/system.html", {
        {"current_time",        String(strTime)},
        {"current_time_iso",    String(strTimeISO)},
        {"ntp_server",          String(config.ntpServer)},
        {"time_zone",           String(config.timeZone)},
        {"wifi_ap_checked",     ((config.wifi_mode == WIFI_AP_STA_FIX) || (config.wifi_mode == WIFI_AP_FIX)) ? String("checked") : String("")},
        {"wifi_ap_ssid",        String(config.wifi_ap_ssid)},
        {"wifi_ap_pass",        String(config.wifi_ap_pass)},
        {"wifi_client_checked", ((config.wifi_mode == WIFI_STA_FIX) || (config.wifi_mode == WIFI_AP_STA_FIX)) ? String("checked") : String("")},
        {"wifi_ssid",           String(config.wifi_ssid)},
        {"wifi_pass",           String(config.wifi_pass)},
        {"wifi_power_options",  wifiPwrOpts},
        {"wifi_mode",           wifiMode},
        {"wifi_mac",            String(WiFi.macAddress())},
        {"wifi_channel",        String(WiFi.channel())},
        {"wifi_tx_power",       String(WiFi.getTxPower())},
        {"wifi_ssid_current",   String(WiFi.SSID())},
        {"wifi_local_ip",       WiFi.localIP().toString()},
        {"wifi_gateway_ip",     WiFi.gatewayIP().toString()},
        {"wifi_dns",            WiFi.dnsIP().toString()}
    });
    wsEnd();
}

extern bool afskSync;
extern String lastPkgRaw;
extern float dBV;
extern int mVrms;
void handle_realtime() {
    // char jsonMsg[1000];
    char *jsonMsg;
    time_t timeStamp;
    time(&timeStamp);

    if (afskSync && (lastPkgRaw.length() > 5)) {
        int input_length = lastPkgRaw.length();
        jsonMsg = (char *)malloc((input_length * 2) + 70);
        char *input_buffer = (char *)malloc(input_length + 2);
        char *output_buffer = (char *)malloc(input_length * 2);
        if (output_buffer) {
            lastPkgRaw.toCharArray(input_buffer, lastPkgRaw.length(), 0);
            lastPkgRaw.clear();
            encode_base64((unsigned char *)input_buffer, input_length,
                          (unsigned char *)output_buffer);
            // Serial.println(output_buffer);
            sprintf(jsonMsg,
                    "{\"Active\":\"1\",\"mVrms\":\"%d\",\"RAW\":\"%s\","
                    "\"timeStamp\":\"%li\"}",
                    mVrms, output_buffer, timeStamp);
            // Serial.println(jsonMsg);
            free(input_buffer);
            free(output_buffer);
        }
    } else {
        jsonMsg = (char *)malloc(100);
        if (afskSync)
            sprintf(jsonMsg,
                    "{\"Active\":\"1\",\"mVrms\":\"%d\",\"RAW\":"
                    "\"REVDT0RFIEZBSUwh\",\"timeStamp\":\"%li\"}",
                    mVrms, timeStamp);
        else
            sprintf(jsonMsg,
                    "{\"Active\":\"0\",\"mVrms\":\"0\",\"RAW\":\"\","
                    "\"timeStamp\":\"%li\"}",
                    timeStamp);
    }
    afskSync = false;
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", String(jsonMsg));
    free(jsonMsg);
}

void handle_test() {
    if (server.hasArg("REBOOT")) {
        esp_restart();
    }
    if (server.hasArg("sendBeacon")) {
        String tnc2Raw = send_gps_location();
        if (config.tnc && tnc2Raw.length() > 0) pkgTxPush(tnc2Raw.c_str(), tnc2Raw.length(), 0);
        // APRS_sendTNC2Pkt(tnc2Raw); // Send packet to RF
    } else if (server.hasArg("sendRaw")) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "raw") {
                if (server.arg(i) != "") {
                    String tnc2Raw = server.arg(i);
                    if (config.tnc) {
                        pkgTxPush(tnc2Raw.c_str(), tnc2Raw.length(), 0);
                        // APRS_sendTNC2Pkt(server.arg(i)); // Send packet to RF
                        // Serial.println("Send RAW: " + tnc2Raw);
                    }
                }
                break;
            }
        }
    }
    setHTML(6);
    String defaultRaw = String(config.aprs_mycall) + "-" + String(config.aprs_ssid) + ">" + APRS_TOCALL + ",WIDE1-1:>Test Status";
    streamTemplate("/test.html", {
        {"default_raw", defaultRaw}
    });
    wsEnd();
}

void handle_firmware() {
    char strCID[50];
    uint64_t chipid = ESP.getEfuseMac();
    sprintf(strCID, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
    setHTML(5);
    streamTemplate("/firmware.html", {
        {"board_name", String(BOARD_NAME)},
        {"version",    String(VERSION_FULL)},
        {"chip_id",    String(strCID)}
    });
    wsEnd();
}

//holds the current upload
File fsUploadFile;
File fsUploadCfgFile;

void handle_configuration() {
    // https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/FSBrowser/FSBrowser.ino
    if (server.hasArg("backupConfig")) {
        log_d("Backup Config");
        char strCID[50];
        uint64_t chipid = ESP.getEfuseMac();
        sprintf(strCID, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

        String myStation;
        if (config.aprs_ssid == 0) {
            myStation = String(config.aprs_mycall);
        } else {
            myStation = String(config.aprs_mycall) + "-" + String(config.aprs_ssid);
        }

        // // BIN
        // String path = "config.bin";
        // String pathOfFileDownload = "APRS-ESP-config_" + myStation + "_" + String(VERSION_FULL) + "_" + String(BOARD_NAME) + "_" + String(strCID) + ".bin";
        // String dataType = "text/plain";

        // LittleFS.begin(true);
        // File myFile = LittleFS.open("/" + path, "r");
        // if (myFile) {
        //     server.sendHeader("Content-Type", dataType);
        //     server.sendHeader("Content-Disposition", "attachment; filename=" + pathOfFileDownload);
        //     server.sendHeader("Connection", "close");
        //     server.streamFile(myFile, "application/octet-stream");
        //     myFile.close();
        // }
        // LittleFS.end();

        // JSON
        String jsonPath = "config.json";
        String jsonPathOfFileDownload = "APRS-ESP-config_" + myStation + "_" + String(VERSION_FULL) + "_" + String(BOARD_NAME) + "_" + String(strCID) + ".json";
        String jsonDataType = "text/plain";

        SaveConfig();
        File jsonFile = LittleFS.open("/" + jsonPath, "r");
        if (jsonFile) {
            String content = jsonFile.readString();
            jsonFile.close();
            server.sendHeader("Content-Disposition", "attachment; filename=\"" + jsonPathOfFileDownload + "\"");
            server.sendHeader("Connection", "close");
            server.send(200, "application/octet-stream", content);
        } else {
            server.send(500, "text/plain", "Failed to write config");
        }
        return;
    }

    char strCID[50];
    uint64_t chipid = ESP.getEfuseMac();
    sprintf(strCID, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
    setHTML(8);
    streamTemplate("/configuration.html", {
        {"board_name", String(BOARD_NAME)},
        {"version",    String(VERSION_FULL)},
        {"chip_id",    String(strCID)}
    });
    wsEnd();
}

void handle_default() {
    DefaultConfig();
    SaveConfig();
    server.sendHeader("Location", "/config", true);
    server.send(302, "text/plain", "");
}

#ifdef SDCARD
void handle_download() {
    String dataType = "text/plain";
    String path;
    if (server.args() > 0) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "FILE") {
                path = server.arg(i);
                break;
            }
        }
    }

    if (path.endsWith(".src"))
        path = path.substring(0, path.lastIndexOf("."));
    else if (path.endsWith(".htm"))
        dataType = "text/html";
    else if (path.endsWith(".csv"))
        dataType = "text/csv";
    else if (path.endsWith(".css"))
        dataType = "text/css";
    else if (path.endsWith(".xml"))
        dataType = "text/xml";
    else if (path.endsWith(".png"))
        dataType = "image/png";
    else if (path.endsWith(".gif"))
        dataType = "image/gif";
    else if (path.endsWith(".jpg"))
        dataType = "image/jpeg";
    else if (path.endsWith(".ico"))
        dataType = "image/x-icon";
    else if (path.endsWith(".svg"))
        dataType = "image/svg+xml";
    else if (path.endsWith(".ico"))
        dataType = "image/x-icon";
    else if (path.endsWith(".js"))
        dataType = "application/javascript";
    else if (path.endsWith(".pdf"))
        dataType = "application/pdf";
    else if (path.endsWith(".zip"))
        dataType = "application/zip";
    else if (path.endsWith(".gz")) {
        if (path.startsWith("/gz/htm"))
            dataType = "text/html";
        else if (path.startsWith("/gz/css"))
            dataType = "text/css";
        else if (path.startsWith("/gz/csv"))
            dataType = "text/csv";
        else if (path.startsWith("/gz/xml"))
            dataType = "text/xml";
        else if (path.startsWith("/gz/js"))
            dataType = "application/javascript";
        else if (path.startsWith("/gz/svg"))
            dataType = "image/svg+xml";
        else
            dataType = "application/x-gzip";
    }
    // webString = "<html><head>\n";
    // webString += "<meta http - equiv = \"content-type\" content =
    // \"text/html; charset=utf-8\" / > \n"; webMessage += "</head><body>\n";
    // webString += "</body></html>\n";
    File myFile = SD.open("/" + path, "r");
    if (myFile) {
        server.sendHeader("Content-Type", dataType);
        server.sendHeader("Content-Disposition",
                          "attachment; filename=" + path);
        server.sendHeader("Connection", "close");
        server.streamFile(myFile, "application/octet-stream");
        myFile.close();
    }
    delay(100);
}

void handle_delete() {
    String dataType = "text/plain";
    String path;
    if (server.args() > 0) {
        for (uint8_t i = 0; i < server.args(); i++) {
            if (server.argName(i) == "FILE") {
                path = server.arg(i);
                Serial.println("Deleting file: " + path);
                if (SD.remove("/" + path)) {
                    Serial.println("File deleted");
                } else {
                    Serial.println("Delete failed");
                }
                break;
            }
        }
    }

    handle_storage();
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.print(file.name());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",
                          (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1,
                          tmstruct->tm_mday, tmstruct->tm_hour,
                          tmstruct->tm_min, tmstruct->tm_sec);
            if (levels) {
                listDir(fs, file.name(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.print(file.size());
            time_t t = file.getLastWrite();
            struct tm *tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",
                          (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1,
                          tmstruct->tm_mday, tmstruct->tm_hour,
                          tmstruct->tm_min, tmstruct->tm_sec);
        }
        file = root.openNextFile();
    }
}
#endif

static bool cfgUpdate = false;


void handle_api_dashboard() {
    char strTime[30];
    char uptimeBuf[30];
    struct tm tmstruct;
    tmstruct.tm_year = 0;
    getLocalTime(&tmstruct, 0);
    sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d",
            (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1,
            tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

    uint64_t tn = esp_timer_get_time() / 1000000ULL;
    sprintf(uptimeBuf, "%llu days %02d:%02d:%02d", tn / 86400,
            (int)(tn % 86400) / 3600, (int)(tn % 3600) / 60, (int)(tn % 60));

    bool gpsValid  = gps.location.isValid() && gps.altitude.isValid() &&
                     gps.speed.isValid() && gps.course.isValid() &&
                     gps.satellites.isValid() && gps.hdop.isValid();
    bool gpsOnline = GpsPktCnt() > 0;

    String gnssRows = "";
    if (gpsOnline) {
        const char *fixStatus = gpsValid ? "FIX OK" : "Waiting for FIX";
        gnssRows += "<tr><td>Status</td><td align=\"right\">" + String(fixStatus) +
                    (gpsValid ? ("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Age " + String(gps.location.age() / 1000) + " s") : "") + "</td></tr>";
        gnssRows += "<tr><td>Coords</td><td align=\"right\">" +
                    (gpsValid ? (String(gps.location.lat(), 6) + "&deg;" + (gps.location.rawLat().negative ? "S" : "N") +
                                 " / " + String(gps.location.lng(), 6) + "&deg;" + (gps.location.rawLng().negative ? "W" : "E")) : "-") + "</td></tr>";
        gnssRows += "<tr><td>Altitude</td><td align=\"right\">" + (gpsValid ? String(gps.altitude.meters(), 1) + " m" : "-") + "</td></tr>";
        gnssRows += "<tr><td>Speed / Course</td><td align=\"right\">" +
                    (gpsValid ? String(gps.speed.kmph(), 1) + " km/h / " + String(gps.course.deg(), 1) + "&deg;" : "-") + "</td></tr>";
        gnssRows += "<tr><td>Sats / HDOP</td><td align=\"right\">" +
                    String(gps.satellites.value()) + " / " + (gpsValid ? String(gps.hdop.hdop(), 1) : "-") + "</td></tr>";
        gnssRows += "<tr><td>GPS / BDS / GLO</td><td align=\"right\">" +
                    String(gnssSatsGPS) + " / " + String(gnssSatsBDS) + " / " + String(gnssSatsGLO) + "</td></tr>";
    } else {
        gnssRows = "<tr><td>Status</td><td align=\"right\">NO GNSS DATA</td></tr>";
    }

    String lastStationsRows = "";
    sort(pkgList, PKGLISTSIZE);
    for (int i = 0; i < PKGLISTSIZE && i <= 20; i++) {
        if (pkgList[i].time > 0) {
            pkgList[i].calsign[10] = 0;
            localtime_r(&pkgList[i].time, &tmstruct);
            sprintf(strTime, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
            lastStationsRows += "<tr><td align=\"left\">" + String(strTime) +
                "</td><td align=\"center\">" + String(pkgList[i].calsign) +
                "</td><td align=\"center\">" + (pkgList[i].channel == 0 ? "RF" : "Net") +
                "</td><td align=\"right\">" + pkgGetType(pkgList[i].type) + "</td></tr>";
        }
    }

    String topSendRows = "";
    sortPkgDesc(pkgList, PKGLISTSIZE);
    for (int i = 0; i < PKGLISTSIZE && i <= 20; i++) {
        if (pkgList[i].time > 0) {
            pkgList[i].calsign[10] = 0;
            topSendRows += "<tr><td align=\"left\">" + String(pkgList[i].calsign) +
                           "</td><td align=\"right\">" + String(pkgList[i].pkg, DEC) + "</td></tr>";
        }
    }

    JsonDocument doc;
    doc["time"]               = strTime;
    doc["cpu_temp"]           = String(temperatureRead(), 1);
    doc["free_heap"]          = String(ESP.getFreeHeap());
    doc["uptime"]             = uptimeBuf;
    doc["wifi_rssi"]          = String(WiFi.RSSI());
    doc["battery_rows"]       = buildBatteryRows();
    doc["gnss_rows"]          = gnssRows;
    doc["stats_all"]          = String(status.allCount) + " / " + String(status.dropCount);
    doc["stats_rf"]           = String(status.rf2inet) + " / " + String(status.inet2rf);
    doc["last_stations_rows"] = lastStationsRows;
    doc["top_send_rows"]      = topSendRows;

    String json;
    serializeJson(doc, json);
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

void handle_api_gps() {
    bool valid = gps.location.isValid();
    JsonDocument doc;
    doc["valid"] = valid;
    if (valid) {
        doc["lat"] = gps.location.lat();
        doc["lon"] = gps.location.lng();
        doc["alt"] = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    }
    String json;
    serializeJson(doc, json);
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

void SerialStatusLog() {
    char strTime[30];
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d",
            tmstruct.tm_year + 1900, tmstruct.tm_mon + 1,
            tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

    uint64_t tn = esp_timer_get_time() / 1000000ULL;

    bool gpsValid  = gps.location.isValid() && gps.altitude.isValid() &&
                     gps.speed.isValid() && gps.course.isValid() &&
                     gps.satellites.isValid() && gps.hdop.isValid();
    bool gpsOnline = GpsPktCnt() > 0;

    JsonDocument doc;
    doc["t"]        = strTime;
    doc["uptime"]   = (uint32_t)tn;
    doc["heap"]     = ESP.getFreeHeap();
    doc["cpu_temp"] = temperatureRead();
    doc["rssi"]     = WiFi.RSSI();
    {
        String ip = WiFi.localIP().toString();
        doc["ip"] = (ip != "0.0.0.0") ? ip : WiFi.softAPIP().toString();
    }

    JsonObject gpsObj = doc["gps"].to<JsonObject>();
    gpsObj["online"] = gpsOnline ? 1 : 0;
    gpsObj["fix"]    = gpsValid  ? 1 : 0;
    if (gpsValid) {
        gpsObj["lat"]  = gps.location.lat();
        gpsObj["lon"]  = gps.location.lng();
        gpsObj["alt"]  = gps.altitude.meters();
        gpsObj["spd"]  = gps.speed.kmph();
        gpsObj["hdg"]  = gps.course.deg();
        gpsObj["age"]  = (uint32_t)(gps.location.age() / 1000);
    }
    gpsObj["sats"]  = (uint32_t)gps.satellites.value();
    gpsObj["hdop"]  = gpsValid ? gps.hdop.hdop() : 0.0f;
    gpsObj["gps_n"] = gnssSatsGPS;
    gpsObj["bds_n"] = gnssSatsBDS;
    gpsObj["glo_n"] = gnssSatsGLO;

    JsonObject batObj = doc["bat"].to<JsonObject>();
#ifdef USE_PMU
    batObj["pct"]    = (int)PMU.getBatteryPercent();
    batObj["mv"]     = (int)PMU.getBattVoltage();
    batObj["vbus"]   = vbusIn ? (int)PMU.getVbusVoltage() : 0;
    batObj["sys_mv"] = (int)PMU.getSystemVoltage();
    batObj["temp"]   = PMU.getTemperature();
    const char *chgState;
    if (!PMU.isBatteryConnect())       chgState = "absent";
    else switch (PMU.getChargerStatus()) {
        case XPOWERS_AXP2101_CHG_TRI_STATE:  chgState = "trickle"; break;
        case XPOWERS_AXP2101_CHG_PRE_STATE:  chgState = "pre";     break;
        case XPOWERS_AXP2101_CHG_CC_STATE:   chgState = "CC";      break;
        case XPOWERS_AXP2101_CHG_CV_STATE:   chgState = "CV";      break;
        case XPOWERS_AXP2101_CHG_DONE_STATE: chgState = "full";    break;
        default: chgState = vbusIn ? "standby" : "discharging";    break;
    }
    batObj["chg"] = chgState;
#else
    batObj["pct"] = batData >= 0 ? batData : -1;
    batObj["usb"] = usbPlugged ? 1 : 0;
#endif

    JsonObject aprsObj = doc["aprs"].to<JsonObject>();
    aprsObj["all"]    = status.allCount;
    aprsObj["drop"]   = status.dropCount;
    aprsObj["rf2in"]  = status.rf2inet;
    aprsObj["in2rf"]  = status.inet2rf;

    JsonObject cfgObj = doc["cfg"].to<JsonObject>();
    cfgObj["call"] = config.aprs_mycall;
    cfgObj["ssid"] = config.aprs_ssid;
    cfgObj["tnc"]  = config.tnc      ? 1 : 0;
    cfgObj["digi"] = config.tnc_digi ? 1 : 0;
    cfgObj["aprs"] = config.aprs     ? 1 : 0;
    cfgObj["wifi"] = config.wifi_mode;
    cfgObj["sb"]       = (config.aprs_beacon == 0) ? 1 : 0;
    cfgObj["bcn"]      = config.aprs_beacon;
    cfgObj["gps_mode"] = config.gps_mode;
    cfgObj["rf2inet"]  = config.rf2inet  ? 1 : 0;
    cfgObj["inet2rf"]  = config.inet2rf  ? 1 : 0;
    cfgObj["path"]     = config.aprs_path;
    cfgObj["symbol"]   = String(config.aprs_table) + String(config.aprs_symbol);
    cfgObj["freq_rx"]  = config.freq_rx;
    cfgObj["freq_tx"]  = config.freq_tx;
    cfgObj["pwr"]      = config.rf_power ? 1 : 0;
    cfgObj["sql"]      = config.sql_level;
    cfgObj["vol"]      = config.volume;

    String json;
    serializeJson(doc, json);
    log_i("[STATUS] %s", json.c_str());
}

// ── LittleFS file browser ─────────────────────────────────────────────────────

static bool isTextFile(const String &path) {
    return path.endsWith(".html") || path.endsWith(".js")   ||
           path.endsWith(".css")  || path.endsWith(".json") ||
           path.endsWith(".txt")  || path.endsWith(".md")   ||
           path.endsWith(".conf") || path.endsWith(".ini")  ||
           path.endsWith(".csv");
}

void handle_files() {
    setHTML(9);
    streamTemplate("/files.html", {});
    wsEnd();
}

void handle_api_files() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    File root = LittleFS.open("/");
    File f    = root.openNextFile();
    while (f) {
        JsonObject obj = arr.add<JsonObject>();
        String name    = String(f.name());
        String path    = name.startsWith("/") ? name : "/" + name;
        obj["name"]   = name.startsWith("/") ? name.substring(1) : name;
        obj["path"]   = path;
        obj["size"]   = (unsigned int)f.size();
        obj["isDir"]  = f.isDirectory();
        obj["isText"] = isTextFile(path);
        f = root.openNextFile();
    }
    String json;
    serializeJson(doc, json);
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", json);
}

void handle_api_file_get() {
    if (!server.hasArg("path")) { server.send(400, "text/plain", "missing path"); return; }
    String path = server.arg("path");
    if (!path.startsWith("/")) path = "/" + path;
    if (!LittleFS.exists(path)) { server.send(404, "text/plain", "not found"); return; }

    String mime = "application/octet-stream";
    if (path.endsWith(".html"))                                       mime = "text/html";
    else if (path.endsWith(".js"))                                    mime = "application/javascript";
    else if (path.endsWith(".css"))                                   mime = "text/css";
    else if (path.endsWith(".json"))                                  mime = "application/json";
    else if (path.endsWith(".txt") || path.endsWith(".md")   ||
             path.endsWith(".conf")|| path.endsWith(".ini")  ||
             path.endsWith(".csv"))                                   mime = "text/plain";

    File f = LittleFS.open(path, "r");
    if (server.hasArg("dl")) {
        mime = "application/octet-stream";
        String fname = path.substring(path.lastIndexOf('/') + 1);
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
    }
    server.sendHeader("Connection", "close");
    server.streamFile(f, mime);
    f.close();
}

void handle_api_file_post() {
    if (!server.hasArg("path") || !server.hasArg("content")) {
        server.send(400, "text/plain", "missing path or content"); return;
    }
    String path = server.arg("path");
    if (!path.startsWith("/")) path = "/" + path;
    File f = LittleFS.open(path, "w");
    if (!f) { server.send(500, "text/plain", "cannot open"); return; }
    f.print(server.arg("content"));
    f.close();
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
}

void handle_api_delete() {
    if (!server.hasArg("path")) { server.send(400, "text/plain", "missing path"); return; }
    String path = server.arg("path");
    if (!path.startsWith("/")) path = "/" + path;
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", LittleFS.remove(path) ? "OK" : "FAIL");
}

static void serveStatic(const char *path, const char *mime) {
    File f = LittleFS.open(path, "r");
    if (!f) {
        server.send(404, "text/plain", "Not found");
        return;
    }
    server.sendHeader("Connection", "close");
    server.sendHeader("Cache-Control", "max-age=86400");
    server.streamFile(f, mime);
    f.close();
}

void webService() {
    LittleFS.begin(false);
    server.close();
    // web client handlers
    server.on("/favicon.ico",    []() { server.send(204); });
    server.on("/style.css",        []() { serveStatic("/style.css",        "text/css"); });
    server.on("/leaflet.min.js",   []() { serveStatic("/leaflet.min.js",   "application/javascript"); });
    server.on("/leaflet.min.css",  []() { serveStatic("/leaflet.min.css",  "text/css"); });
    server.on("/vu.js",            []() { serveStatic("/vu.js",            "application/javascript"); });
    server.on("/radio.js",         []() { serveStatic("/radio.js",         "application/javascript"); });
    server.on("/jquery.min.js",    []() { serveStatic("/jquery.min.js",    "application/javascript"); });
    server.on("/highcharts.js",    []() { serveStatic("/highcharts.js",    "application/javascript"); });
    server.on("/highcharts-more.js", []() { serveStatic("/highcharts-more.js", "application/javascript"); });
    server.on("/firmware.js",        []() { serveStatic("/firmware.js",        "application/javascript"); });
    server.on("/configuration.js",   []() { serveStatic("/configuration.js",   "application/javascript"); });
    server.on("/dashboard.js",         []() { serveStatic("/dashboard.js",         "application/javascript"); });
    server.on("/files.js",             []() { serveStatic("/files.js",             "application/javascript"); });
    server.on("/codemirror.min.js",    []() { serveStatic("/codemirror.min.js",    "application/javascript"); });
    server.on("/codemirror.min.css",   []() { serveStatic("/codemirror.min.css",   "text/css"); });
    server.on("/cm-theme-dracula.min.css", []() { serveStatic("/cm-theme-dracula.min.css", "text/css"); });
    server.on("/cm-mode-js.min.js",    []() { serveStatic("/cm-mode-js.min.js",    "application/javascript"); });
    server.on("/cm-mode-css.min.js",   []() { serveStatic("/cm-mode-css.min.js",   "application/javascript"); });
    server.on("/cm-mode-xml.min.js",   []() { serveStatic("/cm-mode-xml.min.js",   "application/javascript"); });
    server.on("/cm-mode-html.min.js",  []() { serveStatic("/cm-mode-html.min.js",  "application/javascript"); });
    server.on("/", handle_root);
    server.on("/api/dashboard", HTTP_GET,  handle_api_dashboard);
    server.on("/api/gps",       HTTP_GET,  handle_api_gps);
    server.on("/files",         HTTP_GET,  handle_files);
    server.on("/api/files",     HTTP_GET,  handle_api_files);
    server.on("/api/file",      HTTP_GET,  handle_api_file_get);
    server.on("/api/file",      HTTP_POST, handle_api_file_post);
    server.on("/api/delete",    HTTP_POST, handle_api_delete);
    server.on("/api/upload", HTTP_POST,
        []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", fsUploadFile ? "OK" : "FAIL");
        },
        []() {
            HTTPUpload &upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                String fn = upload.filename;
                if (!fn.startsWith("/")) fn = "/" + fn;
                LittleFS.remove(fn);
                fsUploadFile = LittleFS.open(fn, "w");
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
            } else if (upload.status == UPLOAD_FILE_END) {
                if (fsUploadFile) fsUploadFile.close();
            }
        }
    );
    server.on("/config", handle_setting);
#ifdef SDCARD
    server.on("/data", handle_storage);
    server.on("/download", handle_download);
    server.on("/delete", handle_delete);
#endif
    server.on("/radio", handle_radio);
    server.on("/default", handle_default);
    server.on("/service", handle_service);
    server.on("/system", handle_system);
    server.on("/test", handle_test);
    server.on("/realtime", handle_realtime);
    server.on("/firmware", handle_firmware);
    server.on("/configuration", handle_configuration);
    /* handling uploading configuration file */
    server.on(
        "/updateconfig", HTTP_POST,
        []() {
            log_d("Update config file B");
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", cfgUpdate ? "FAIL" : "OK");
            if (cfgUpdate) LoadReConfig();
            cfgUpdate = false;
        },
        []() {
            log_d("Update config file A");
            HTTPUpload &upload = server.upload();
            log_d("Upload Status: %d", upload.status);
            if (upload.status == UPLOAD_FILE_START) {
                String filenameJson = upload.filename;
                filenameJson = "tconfig.json";    // override filename
                if (!filenameJson.startsWith("/")) 
                    filenameJson = "/" + filenameJson;
                log_d("handleFileUpload Name: %s", filenameJson.c_str());
                // LittleFS.remove(filename);
                fsUploadCfgFile = LittleFS.open(filenameJson, "w");
                filenameJson = String();
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                log_d("handleFileUpload Data: %d", upload.currentSize);
                if (fsUploadCfgFile) fsUploadCfgFile.write(upload.buf, upload.currentSize);
            } else if (upload.status == UPLOAD_FILE_END) {
                log_d("handleFileUpload Size: %d", upload.totalSize);
                if (fsUploadCfgFile) {
                    fsUploadCfgFile.close();
                    fsUploadCfgFile = LittleFS.open("/tconfig.json", "r");
                    if (fsUploadCfgFile) {
                        size_t cfgSize = fsUploadCfgFile.size();
                        fsUploadCfgFile.close();
                        log_d("filesize: %d", cfgSize);
                        if (upload.totalSize > 0) {
                            if (upload.totalSize == cfgSize) {
                                log_d("Update Success");
                                if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");
                                LittleFS.rename("/tconfig.json", "/config.json");
                                cfgUpdate = true;
                            } else {
                                log_d("Update Fail");
                            }
                        }
                    }
                }
            }
        }
    );
    /* handling uploading firmware file */
    server.on(
        "/update", HTTP_POST,
        []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart();
        },
        []() {
            HTTPUpload &upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                log_i("Firmware Update FILE: %s", upload.filename.c_str());
                bool isFS = upload.filename.indexOf("littlefs") >= 0;
                int updateType = isFS ? U_SPIFFS : U_FLASH;
                log_i("Update type: %s", isFS ? "LittleFS" : "Firmware");
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, updateType)) {
                    Update.printError(Serial);
                    delay(3);
                } else {
                    disableCore0WDT();
                    disableCore1WDT();
                    disableLoopWDT();

                    vTaskSuspend(taskAPRSHandle);
                    config.aprs = false;
                    config.tnc = false;

#ifndef I2S_INTERNAL
                    AFSK_TimerEnable(false);
#endif
                    fwUpdateProcess = true;

                    delay(3);
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                /* flashing firmware to ESP*/                
                // log_i("Firmware Update Data: %d", upload.currentSize);
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                    delay(3);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                log_i("Firmware Update Size: %d", upload.totalSize);
                if (Update.end(true)) {  // true to set the size to the current
                                         // progress
                    delay(3);
                } else {
                    Update.printError(Serial);
                    delay(3);
                }
            }
        });
    // Captive portal probe URLs — always redirect to the device UI.
    // Never return Success: macOS treats a Success response as "internet
    // detected" and immediately shows "Open in Safari?" over the loaded page.
    server.on("/hotspot-detect.html",       []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.sendHeader("Connection", "close");
        server.send(302, "text/plain", "");
    });
    server.on("/library/test/success.html", []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.sendHeader("Connection", "close");
        server.send(302, "text/plain", "");
    });
    server.on("/connecttest.txt",           []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.sendHeader("Connection", "close");
        server.send(302, "text/plain", "");
    });
    server.on("/generate_204",              []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.sendHeader("Connection", "close");
        server.send(302, "text/plain", "");
    });
    server.on("/gen_204",                   []() {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.sendHeader("Connection", "close");
        server.send(302, "text/plain", "");
    });
    server.onNotFound([]() {
        String path = server.uri();
        if (path.startsWith("/static/") && path.endsWith(".png") && LittleFS.exists(path)) {
            serveStatic(path.c_str(), "image/png");
            return;
        }
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
        server.send(302, "text/plain", "Redirect");
    });

    server.begin();

    if (config.wifi_mode & WIFI_AP_FIX) {
        dnsServer.stop();  // stop before restart in case called twice
        dnsServer.setTTL(300);
        dnsServer.start(53, "*", WiFi.softAPIP());
        log_i("Captive portal DNS started on %s", WiFi.softAPIP().toString().c_str());
    }
}
