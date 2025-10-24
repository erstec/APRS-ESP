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
#include "base64.hpp"
#include "utilities.h"
#include <LibAPRSesp.h>
#include <TinyGPS++.h>
#include "gps.h"

WebServer server(80);

bool defaultSetting = false;

static int batData = 0;
static bool usbPlugged = false;

extern TinyGPSPlus gps;

void WebDataUpdate(int _batData, bool _usbPlugged) {
    batData = _batData;
    usbPlugged = _usbPlugged;
}

void serviceHandle() { server.handleClient(); }

void sendHTMLHeader(byte page) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent("<html><head>\n");
    server.sendContent("<meta charset=\"utf-8\">");
    server.sendContent("<style>\n");
    server.sendContent("body{background-color: #5f5f5f;font-family:\"Segoe UI\",Tahoma,sans-serif;color:#f1f1f1;font-weight: normal;display: inline-block;}\n");
    server.sendContent("hdr1{background-color: powderblue;color: white;vertical-align: middle;text-align: center;font-size: 16px;font-weight: bold;}\n");
    server.sendContent("a{color: powderblue; text-decoration: none;}\n");
    server.sendContent(".topnav{position:relative;z-index:2;font-size:25px;background-color:#5f5f5f;color:#f1f1f1;width:100%;padding:10px;letter-spacing:3px;box-shadow:0 10px 10px 0 rgba(0,0,0,0.16);font-family:\"Segoe UI\",Tahoma,sans-serif;}\n");
    server.sendContent(".L1{text-align: center;width: 50%;margin: 1px;background: darkgray;color: white;font-size: 15px;font-family:\"Segoe UI\",Tahoma,sans-serif;border-radius: 0px;font-weight: bold;}\n");
    server.sendContent(".form-control{display:block;width:100%;height:22px;padding:6px 5px;font-size:14px;line-height:1.42857143;color:#555;background-color:#fff;background-image:none;border:1px solid #ccc;border-radius:0px;}\n");
    server.sendContent(".btn{display:inline-block;margin-bottom:0;font-weight:400;text-align:center;vertical-align:middle;cursor:pointer;background-image:none;border:1px solid transparent;white-space:nowrap;padding:0px 10px;font-size:14px;line-height:1.42857143;border-radius:0px;}\n");
    server.sendContent(".btn-primary{color:#fff;background-color:#428bca;border-color:#357ebd;}\n");
    server.sendContent(".btn-danger{color:#fff;background-color:#d9534f;border-color:#d43f3a;}\n");
    server.sendContent(".nav-tabs>li.active>a,.nav-tabs>li.active>a:hover,.nav-tabs>li.active>a:focus{color:#428bca;background-color:#e5e5e5;border:1px solid #ddd;border-bottom-color:transparent;cursor:default;}\n");
    server.sendContent(".nav-tabs>li>a{margin-right:0px;line-height:1.42857143;border:1px solid #ddd;border-radius:0 0 0 0;text-decoration:none;color:darkgray;}\n");
    server.sendContent("</style>\n");
    if (page == 0) server.sendContent("<meta http-equiv=\"refresh\" content=\"10;url=http://" + WiFi.localIP().toString() + "\"> \n");
    server.sendContent("<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" /> \n");
}

void sendNav(byte page) {
    String strActive[9];
    for(int i=0; i<9; i++) strActive[i] = "";
    if(page < 9) strActive[page] = "class=active";

    String myStation = String(config.aprs_mycall);
    if (config.aprs_ssid != 0) myStation += "-" + String(config.aprs_ssid);

    server.sendContent("</head><body>\n");
    server.sendContent("<div class='topnav'><b>APRS-ESP32 Internet Gateway - " + myStation + "</b></div>\n");
    server.sendContent("<div class=\"row\"><ul class=\"nav nav-tabs\" style=\"margin-top: 10px;margin-bottom: 10px;\">\n");
    server.sendContent("<li role=\"presentation\" " + strActive[0] + "><a href=\"/\">Dash Board</a></li>\n");
    #ifdef SDCARD
    server.sendContent("<li role=\"presentation\" " + strActive[1] + "><a href=\"/data\">Storage</a></li>\n");
    #endif
    server.sendContent("<li role=\"presentation\" " + strActive[2] + "><a href=\"/config\">Setting</a></li>\n");
    server.sendContent("<li role=\"presentation\" " + strActive[7] + "><a href=\"/radio\">Radio</a></li>\n");
    server.sendContent("<li role=\"presentation\" " + strActive[3] + "><a href=\"/service\">Service</a></li>\n");
    server.sendContent("<li role=\"presentation\" " + strActive[4] + "><a href=\"/system\">System</a></li>\n");
    server.sendContent("<li role=\"presentation\" " + strActive[6] + "><a href=\"/test\">Test</a></li>\n");
    server.sendContent("<li role=\"presentation\" " + strActive[5] + "><a href=\"/firmware\">Firmware</a></li>\n");
    server.sendContent("<li role=\"presentation\" " + strActive[8] + "><a href=\"/configuration\">Configuration</a></li>\n");
    server.sendContent("</ul></div>\n");
}

void handle_root() {
    sendHTMLHeader(0);
    sendNav(0);

    char strTime[30];
    struct tm tmstruct;
    time_t tn = now() - systemUptime;
    getLocalTime(&tmstruct, 0);
    sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d", (tmstruct.tm_year)+1900, (tmstruct.tm_mon)+1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

    server.sendContent("<table style=\"width:800px;\"><tr><td>");
    server.sendContent("<div style=\"width:300px\"><b>Last Readings at " + String(strTime) + "</b></div>\n");
    server.sendContent("<span>&nbsp;</span>\n");
    server.sendContent("<div>CPU Temp: " + String(temperatureRead(), 1) + "&deg;C</div>\n");
    server.sendContent("<div>Free Heap: " + String(ESP.getFreeHeap()) + " bytes</div>\n");
    sprintf(strTime, "%d days %02d:%02d:%02d", day(tn)-1, hour(tn), minute(tn), second(tn));
    server.sendContent("<div>System Uptime: " + String(strTime) + "</div>\n");
    server.sendContent("<div>WiFi RSSI: " + String(WiFi.RSSI()) + " dBm</div>\n");

    String batStr = (batData > 0) ? String(batData) + "%" : "N/A";
    String usbStr = usbPlugged ? " - USB Plugged" : "";
    server.sendContent("<div>Battery: " + batStr + " " + usbStr + "</div>\n");

    server.sendContent("</td></tr><tr align=\"left\"><td>\n");

    bool gpsValid = gps.location.isValid() && gps.altitude.isValid() && gps.speed.isValid() && gps.course.isValid() && gps.satellites.isValid() && gps.hdop.isValid();
    bool gpsOnline = GpsPktCnt() > 0;

    server.sendContent("<div class=\"L1\">GNSS STATUS</div><table border=\"0\" width=\"400\">");
    if (gpsOnline) {
        server.sendContent("<tr><td>Valid Fix</td><td align=\"right\">" + String(gpsValid ? "YES" : "NO") + "&nbsp;&nbsp;Age " + (gpsValid ? String(gps.location.age()/1000) + " s" : "-") + "</td></tr>");
        server.sendContent("<tr><td>Coords</td><td align=\"right\">" + (gpsValid ? String(gps.location.lat(), 6) + "&deg; / " + String(gps.location.lng(), 6) + "&deg;" : "- / -") + "</td></tr>");
    } else {
        server.sendContent("<tr><td>NO GPS DATA</td><td></td></tr>");
    }
    server.sendContent("</table>\n");

    server.sendContent("<div class=\"L1\">STATISTICS</div><table border=\"0\" width=\"400\">");
    server.sendContent("<tr><td>ALL DATA / DROP</td><td align=\"right\">" + String(status.allCount) + " / " + String(status.dropCount) + "</td></tr>");
    server.sendContent("<tr><td>RF->INET / INET->RF</td><td align=\"right\">" + String(status.rf2inet) + " / " + String(status.inet2rf) + "</td></tr>");
    server.sendContent("</table>\n");

    server.sendContent("<div class=\"L1\">LAST STATIONS</div><table border=\"0\" width=\"400\">");
    sort(pkgList, PKGLISTSIZE);
    for (int i = 0; i < PKGLISTSIZE; i++) {
        if (pkgList[i].time > 0) {
            pkgList[i].calsign[10] = 0;
            localtime_r(&pkgList[i].time, &tmstruct);
            sprintf(strTime, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
            server.sendContent("<tr><td>" + String(strTime) + "</td><td>" + String(pkgList[i].calsign) + "</td><td>" + (pkgList[i].channel==0 ? "RF" : "Net") + "</td><td>" + pkgGetType(pkgList[i].type) + "</td></tr>");
        }
    }
    server.sendContent("</table>\n");
    server.sendContent("</td></tr></table></body></html>");
    server.sendContent("");
}
#ifdef SDCARD
void handle_storage() {
    sendHTMLHeader(1);
    sendNav(1);
    // Simplified for now
    server.sendContent("SD Card not fully implemented in this version.");
    server.sendContent("</body></html>");
    server.sendContent("");
}
#endif

void handle_setting() {
    // Handling of form submission should be added here
    sendHTMLHeader(2);
    sendNav(2);
    server.sendContent("<div class=\"col-xs-10\"><form action=\"/config\" method=\"post\">");
    server.sendContent("<h3>Fix Location</h3>");
    server.sendContent("<div class=\"form-group\"><label>Latitude</label><input class=\"form-control\" name=\"gpsLat\" type=\"text\" value=\"" + String(config.gps_lat, 5) + "\"></div>");
    server.sendContent("<div class=\"form-group\"><label>Longitude</label><input class=\"form-control\" name=\"gpsLon\" type=\"text\" value=\"" + String(config.gps_lon, 5) + "\"></div>");
    // Add all other settings fields as above
    server.sendContent("<input class=\"btn btn-primary\" type=\"submit\" value=\"Save Config\">");
    server.sendContent("</form></div></body></html>");
    server.sendContent("");
}
void handle_service() {
    sendHTMLHeader(3);
    sendNav(3);
    server.sendContent("<div class=\"col-xs-10\"><form action=\"/service\" method=\"post\">");
    server.sendContent("<h3>APRS Setting</h3>");
    server.sendContent("<div class=\"form-group\"><label>APRS-IS Enable</label><input type=\"checkbox\" name=\"aprsEnable\" value=\"OK\" " + String(config.aprs ? "checked" : "") + "></div>");
    server.sendContent("<div class=\"form-group\"><label>My CallSign</label><input class=\"form-control\" name=\"myCall\" type=\"text\" value=\"" + String(config.aprs_mycall) + "\"></div>");
    // Add all other service fields
    server.sendContent("<input class=\"btn btn-primary\" type=\"submit\" value=\"Save Config\">");
    server.sendContent("</form></div></body></html>");
    server.sendContent("");
}
void handle_radio() {
    sendHTMLHeader(7);
    sendNav(7);
    server.sendContent("<div class=\"col-xs-10\"><form action=\"/radio\" method=\"post\">");
    server.sendContent("<h3>RF Module SA818/SA868</h3>");
    server.sendContent("<div class=\"form-group\"><label>TX Frequency</label><input class=\"form-control\" name=\"tx_freq\" type=\"number\" step=\"0.0125\" value=\""+String(config.freq_tx, 4)+"\"></div>");
    server.sendContent("<div class=\"form-group\"><label>RX Frequency</label><input class=\"form-control\" name=\"rx_freq\" type=\"number\" step=\"0.0125\" value=\""+String(config.freq_rx, 4)+"\"></div>");
    // Add all other radio fields
    server.sendContent("<input class=\"btn btn-primary\" type=\"submit\" value=\"Save Config\">");
    server.sendContent("</form></div></body></html>");
    server.sendContent("");
}
void handle_system() {
    sendHTMLHeader(4);
    sendNav(4);
    char strTime[20];
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d", (tmstruct.tm_year)+1900, (tmstruct.tm_mon)+1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);

    server.sendContent("<div class=\"col-pad\"><h3>Time Setting</h3><p>Current Time: "+String(strTime)+"</p>");
    // Forms for time update would go here
    server.sendContent("<h3>WiFi Network</h3>");
    // Forms for wifi update
    server.sendContent("</div></body></html>");
    server.sendContent("");
}
void handle_test() {
    sendHTMLHeader(6);
    sendNav(6);
    server.sendContent("<form action=\"/test\" method=\"post\">");
    server.sendContent("<input type=\"submit\" name=\"sendBeacon\" value=\"SEND BEACON\" class=\"btn btn-danger\">");
    server.sendContent("</form>");
    server.sendContent("<div id=\"vumeter\"></div>");
    server.sendContent("<textarea id=\"raw_txt\" rows=\"25\" cols=\"80\"></textarea>");
    server.sendContent("</body></html>");
    server.sendContent("");
}
void handle_firmware() {
    sendHTMLHeader(5);
    sendNav(5);
    server.sendContent("<h3>Firmware Update</h3>");
    server.sendContent("<form method='POST' action='/update' enctype='multipart/form-data'>");
    server.sendContent("<input type='file' name='update'><input type='submit' value='Update'>");
    server.sendContent("</form></body></html>");
    server.sendContent("");
}
void handle_configuration() {
    sendHTMLHeader(8);
    sendNav(8);
    server.sendContent("<h3>Configuration Backup/Restore</h3>");
    server.sendContent("<form method='POST' action='/configuration'><input class=\"btn\" type=\"submit\" name=\"backupConfig\" value=\"Config Backup\"></form>");
    server.sendContent("<form method='POST' action='/updateconfig' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Config Restore'></form>");
    server.sendContent("</body></html>");
    server.sendContent("");
}
void handle_default() {
    defaultSetting = true;
    DefaultConfig();
    handle_setting();
    defaultSetting = false;
}
extern bool afskSync;
extern String lastPkgRaw;
extern int mVrms;
void handle_realtime() {
    time_t timeStamp;
    time(&timeStamp);
    String json = "{\"Active\":\"0\",\"mVrms\":\"0\",\"RAW\":\"\",\"timeStamp\":\"" + String(timeStamp) + "\"}";
    if (afskSync && lastPkgRaw.length() > 5) {
        char* output_buffer = (char*)malloc(lastPkgRaw.length() * 2); // Allocate a larger buffer as a safe fallback
        encode_base64((unsigned char*)lastPkgRaw.c_str(), lastPkgRaw.length(), (unsigned char*)output_buffer);
        json = "{\"Active\":\"1\",\"mVrms\":\"" + String(mVrms) + "\",\"RAW\":\"" + String(output_buffer) + "\",\"timeStamp\":\"" + String(timeStamp) + "\"}";
        free(output_buffer);
        lastPkgRaw = "";
    }
    server.send(200, "application/json", json);
}

void webService() {
    server.close();
    server.on("/", handle_root);
    server.on("/config", handle_setting);
#ifdef SDCARD
    server.on("/data", handle_storage);
#endif
    server.on("/radio", handle_radio);
    server.on("/default", handle_default);
    server.on("/service", handle_service);
    server.on("/system", handle_system);
    server.on("/test", handle_test);
    server.on("/realtime", handle_realtime);
    server.on("/firmware", handle_firmware);
    server.on("/configuration", handle_configuration);
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.println("Update Success");
            } else {
                Update.printError(Serial);
            }
        }
    });
    server.on("/updateconfig", HTTP_POST, [](){
        server.send(200, "text/plain", "Not implemented");
    });
    server.begin();
}
