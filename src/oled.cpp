
/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for work with OLED Display.
    Author:         Ernest / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#include "oled.h"
#include "AFSK.h"
#include "TinyGPS++.h"
#include "WiFi.h"
#include "config.h"
#include "gps.h"
#include "main.h"
#include "rotaryProc.h"
#include "pkgList.h"
#include "aprsMsg.h"
#include "aprs_symbols_bmp.h"

#include <Fonts/FreeSans9pt7b.h>

#include <Wire.h>
#if defined(USE_SCREEN_SSD1306)
#include "Adafruit_SSD1306.h"
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RST_PIN);
#elif defined(USE_SCREEN_SH1106)
#include <Adafruit_SH1106.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
Adafruit_SH1106 display(OLED_SDA_PIN, OLED_SCL_PIN);
#endif

extern TinyGPSPlus gps;
extern Configuration config;
extern WiFiClient aprsClient;
#ifdef USE_PMU
#include <XPowersLib.h>
extern XPowersPMU PMU;
extern bool vbusIn;
#endif

#define POPUP_QUEUE_SIZE 5
struct PopupEntry {
    char    caption[16];
    char    msg[16];
    char    msg2[16];
    uint8_t timeout;
    int8_t  bar;       // -1 = none, 0-100 = percentage bar
    bool    dualFont;  // true = both lines in FreeSans9pt7b, no caption
};
static PopupEntry popupQueue[POPUP_QUEUE_SIZE];
static int popupHead  = 0;
static int popupCount = 0;

static unsigned long oledActivityMs = 0;
static bool          oledDimmed     = false;
static bool          oledOn         = true;   // false = user-toggled off; skip display() to prevent lib re-enable

static PopupEntry *popupCurrent() { return &popupQueue[popupHead % POPUP_QUEUE_SIZE]; }

// enqueue=false (default): transient/replace — skipped if an important popup (timeout>=5) is active
// enqueue=true:            alert — always added to back, for SMS-type messages
static void popupPush(const PopupEntry &e, bool enqueue = false) {
    if (enqueue) {
        if (popupCount < POPUP_QUEUE_SIZE)
            popupQueue[(popupHead + popupCount++) % POPUP_QUEUE_SIZE] = e;
        return;
    }
    // transient: skip if important popup is at head
    if (popupCount > 0 && popupCurrent()->timeout >= 5) return;
    // replace: reset queue
    popupHead  = 0;
    popupCount = 1;
    popupQueue[0] = e;
}

void OledStartup() {
#ifdef USE_SCREEN
#if defined(USE_SCREEN_SSD1306)
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false)) {
        log_e("SSD1306 init failed");
    } else {
        log_i("SSD1306 init ok");
    }
#elif defined(USE_SCREEN_SH1106)
    log_i("SH1106 init");
    display.begin(SH1106_SWITCHCAPVCC, 0x3C, false);
    display.stopscroll();
#endif

    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);
    display.setTextSize(1);
    display.setTextWrap(false);
    display.cp437(true);

    char buf[16];
    sprintf(buf, "APRS-ESP V%s", VERSION);
    display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, 0);
    display.print(buf);

    sprintf(buf, "Boot...");
    display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, CHAR_HEIGHT * 2);
    display.print(buf);

    display.display();
#endif
}

void OledPostStartup(int step, int total) {
    const int barH = 8;
    const int barY = display.height() - barH - 2;   // 54
    const int barX = 4;
    const int barW = display.width() - barX * 2;    // 120

    display.fillRect(0, barY - CHAR_HEIGHT - 2, display.width(), CHAR_HEIGHT + barH + 4, BLACK);

    const char *label = (step >= total) ? "RF Init... OK" : "RF Init...";
    display.setCursor(display.width() / 2 - strlen(label) * CHAR_WIDTH / 2, barY - CHAR_HEIGHT - 1);
    display.print(label);

    display.drawRect(barX, barY, barW, barH, WHITE);
    if (step > 0) {
        int fill = (step * (barW - 2)) / total;
        display.fillRect(barX + 1, barY + 1, fill, barH - 2, WHITE);
    }

    display.display();
}

void OledPostStartup(const char* msg) {
#ifdef USE_SCREEN
    display.fillRect(0, CHAR_HEIGHT * 4 - 2, display.width(), CHAR_HEIGHT * 5, BLACK);
    display.setCursor(display.width() / 2 - strlen(msg) * CHAR_WIDTH / 2, CHAR_HEIGHT * 4 - 2);
    display.print(msg);
    display.display();
#endif
}

// draw rectangle box with message (queue-aware)
static void OledDrawMsg(bool countDown = true) {
    // auto-advance any expired entries
    while (popupCount > 0 && popupCurrent()->timeout == 0) {
        popupHead = (popupHead + 1) % POPUP_QUEUE_SIZE;
        popupCount--;
    }
    if (popupCount == 0) return;

    PopupEntry *cur = popupCurrent();

    display.fillRect(10 - 2, 5 - 2, display.width() - 20 + 2, display.height() - 10 + 2, BLACK);
    display.drawRect(10, 5, display.width() - 20, display.height() - 10, WHITE);

    int16_t x1, y1;
    uint16_t w, h;

    if (cur->dualFont) {
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds(cur->msg, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((display.width() - (int16_t)w) / 2, 26);
        display.print(cur->msg);
        display.getTextBounds(cur->msg2, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((display.width() - (int16_t)w) / 2, 46);
        display.print(cur->msg2);
        display.setFont();
    } else {
        display.setCursor(display.width() / 2 - strlen(cur->caption) * CHAR_WIDTH / 2, 10);
        display.print(cur->caption);
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds(cur->msg, 0, 0, &x1, &y1, &w, &h);
        if (w > display.width() - 20) w = display.width() - 20;
        display.setCursor(display.width() / 2 - w / 2, 35);
        display.print(cur->msg);
        display.setFont();
        display.setCursor(display.width() / 2 - strlen(cur->msg2) * CHAR_WIDTH / 2, 42);
        display.print(cur->msg2);

        if (cur->bar >= 0) {
            int barX = 14, barY = 51, barH = 5;
            int barW = display.width() - barX * 2;
            int fill = (int)((long)cur->bar * barW / 100);
            display.drawRect(barX, barY, barW, barH, WHITE);
            if (fill > 0) display.fillRect(barX, barY, fill, barH, WHITE);
        }
    }

    if (countDown && cur->timeout > 0) cur->timeout--;
}

// 8×7 PROGMEM icons for main screen (MSB = leftmost pixel)
// 8×7 tilted satellite — diagonal body (3×3) with solar panels on each corner
static const uint8_t PROGMEM icon_sat[] = {
    0x06, // . . . . . 1 1 .  right panel top
    0x0C, // . . . . 1 1 . .  right panel bottom
    0x38, // . . 1 1 1 . . .  body
    0x38, // . . 1 1 1 . . .  body
    0x38, // . . 1 1 1 . . .  body
    0x60, // . 1 1 . . . . .  left panel top
    0xC0, // 1 1 . . . . . .  left panel bottom
};
static const uint8_t PROGMEM icon_clock[] = {
    0x18, // . . . 1 1 . . .  stopwatch crown
    0x3C, // . . 1 1 1 1 . .  top arc
    0x42, // . 1 . . . . 1 .  sides
    0x4A, // . 1 . . 1 . 1 .  hand start → lower-right (4-5 o'clock)
    0x46, // . 1 . . . 1 1 .  hand tip one row lower
    0x42, // . 1 . . . . 1 .  sides
    0x3C, // . . 1 1 1 1 . .  bottom arc
};
static const uint8_t PROGMEM icon_dist[] = {
    0x80, // 1 . . . . . . .  | top
    0x84, // 1 . . . . 1 . .  | upper chevron
    0x82, // 1 . . . . . 1 .  | outer tip
    0xFF, // 1 1 1 1 1 1 1 1  |—— shaft
    0x82, // 1 . . . . . 1 .  | outer tip
    0x84, // 1 . . . . 1 . .  | lower chevron
    0x80, // 1 . . . . . . .  | bottom
};

// WiFi signal bars (8×6) — 3 bars of increasing height. connected=all bars, disconnected=none.
static const uint8_t PROGMEM wifi_on_bmp[] = {
    0x03, // . . . . . . 1 1  right bar top
    0x03, // . . . . . . 1 1
    0x1B, // . . . 1 1 . 1 1  middle + right bar
    0x1B, // . . . 1 1 . 1 1
    0xDB, // 1 1 . 1 1 . 1 1  all bars
    0xDB, // 1 1 . 1 1 . 1 1
};

static const uint8_t PROGMEM wifi_off_bmp[] = {
    0x05, // . . . . . 1 . 1  X mark
    0x02, // . . . . . . 1 .
    0x05, // . . . . . 1 . 1
    0x00, // . . . . . . . .
    0x00, // . . . . . . . .
    0xDB, // 1 1 . 1 1 . 1 1  bottom bar only
};

static void drawWifiIcon(int16_t x, int16_t y, bool connected) {
    display.drawBitmap(x, y, connected ? wifi_on_bmp : wifi_off_bmp, 8, 6, WHITE);
}

// 12×6 battery icon; charging = lightning bolt in body instead of fill bar
static void drawBatIcon(int16_t x, int16_t y, int pct, bool chg = false) {
    display.drawRect(x, y, 10, 6, WHITE);
    display.fillRect(x + 10, y + 1, 2, 4, WHITE);
    if (chg) {
        // ZigZag lightning bolt, right-center of body
        display.drawPixel(x + 5, y + 1, WHITE);
        display.drawPixel(x + 6, y + 1, WHITE);
        display.drawPixel(x + 5, y + 2, WHITE);
        display.drawPixel(x + 4, y + 3, WHITE);
        display.drawPixel(x + 5, y + 3, WHITE);
        display.drawPixel(x + 4, y + 4, WHITE);
    } else {
        int fill = pct * 8 / 100;
        if (fill > 0) display.fillRect(x + 1, y + 1, fill, 4, WHITE);
    }
}

void OledUpdate(int batData, bool usbPlugged, bool afskInit, bool charging) {
#ifdef USE_SCREEN
    if (!oledOn) return;
    if (menuMode != MENU_HIDDEN) {
        OledDrawMenu();
        return;
    }
    if (afskInit) {
        if (AFSK_modem->sending) return;
        // if (AFSK_modem->hdlc.receiving) return;
    }

    char buf[22];

    bool isValid = gps.location.isValid();
    uint32_t satCnt = gps.satellites.value();

    display.clearDisplay();

    // ── Row y=0: Callsign (left) + A± + WiFi icon + battery icon (right) ──
    display.setCursor(0, 0);
    if (config.aprs_ssid > 0)
        display.printf("%s-%d", config.aprs_mycall, config.aprs_ssid);
    else
        display.print(config.aprs_mycall);

    const int16_t kBatW  = 13;  // 10 body + 2 terminal + 1 gap
    const int16_t kWifiW = 9;   // 8px icon + 1px gap
    int16_t batX  = display.width() - kBatW;
    int16_t wifiX = (config.wifi_mode != WIFI_OFF) ? batX - kWifiW : batX;
    int16_t aprsX = config.aprs ? wifiX - 2 * CHAR_WIDTH : wifiX;

    if (config.wifi_mode != WIFI_OFF)
        drawWifiIcon(wifiX, 1, WiFi.status() == WL_CONNECTED);
    if (config.aprs) {
        display.setCursor(aprsX, 0);
        display.print(aprsClient.connected() ? "A+" : "A-");
    }

    // Battery icon
    int batPct = 0;
#if defined(ADC_BATTERY)
    if (config.wifi_mode == WIFI_OFF) {
        batPct = (batData >= 0) ? batData : 0;
    } else {
        batPct = usbPlugged ? 100 : 0;
    }
#else
    batPct = (batData >= 0) ? batData : 0;
#endif
    drawBatIcon(batX, 1, batPct, charging);

    // ── Large zone: Speed (left) + Heading (right), FreeSans9pt7b ─────────
    display.setFont(&FreeSans9pt7b);

    snprintf(buf, sizeof(buf), "%d km/h", (int)(satCnt > 0 ? gps.speed.kmph() : 0));
    display.setCursor(0, 25);
    display.print(buf);

    char hdgNum[5];
    if (isValid)
        snprintf(hdgNum, sizeof(hdgNum), "%d", (int)gps.course.deg());
    else
        strlcpy(hdgNum, "---", sizeof(hdgNum));
    int16_t hx1, hy1; uint16_t hw, hh;
    display.getTextBounds(hdgNum, 0, 0, &hx1, &hy1, &hw, &hh);
    // flush right: digits + 3px gap + ° (CHAR_WIDTH) = right edge
    int16_t hdgX = display.width() - (int16_t)hw - CHAR_WIDTH - 3;
    display.setCursor(hdgX, 25);
    display.print(hdgNum);
    display.setFont();

    if (isValid) {
        display.setCursor(hdgX + (int16_t)hw + 3, 12);
        display.print('\xF8');
    }

    // ── Row y=32: Latitude + Longitude ───────────────────────────────────
    display.setCursor(0, 32);
    display.print(deg_to_nmea(lat, true));
    display.print(" ");
    display.print(deg_to_nmea(lon, false));

    // ── Row y=40: QTH locator + Time + Altitude ──────────────────────────
    display.setCursor(0, 40);
    display.print(isValid ? deg_to_qth(lat, lon) : "------");

    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    bool timeBad = (tmstruct.tm_hour > 24 || tmstruct.tm_min > 59 || tmstruct.tm_sec > 59);
    if (timeBad) {
        strlcpy(buf, "NO TIME ", sizeof(buf));
    } else {
        snprintf(buf, 9, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
        buf[8] = 0;
    }
    display.setCursor(43, 40);
    display.print(buf);

    if (isValid) {
        snprintf(buf, sizeof(buf), "%.0fm", gps.altitude.meters());
        display.setCursor(display.width() - (int16_t)(strlen(buf) * CHAR_WIDTH), 40);
        display.print(buf);
    }

    // ── Row y=48: sat icon + sats + timesync (left)  TX interval + flags (right)
    display.drawBitmap(0, 48, icon_sat, 8, 7, WHITE);
    display.setCursor(10, 48);
    display.printf("%lu%s%s", (unsigned long)satCnt,
                   isValid ? "+" : "-", GpsPktCnt() > 0 ? "+" : "-");
    switch (timeSyncFlag) {
        case T_SYNC_NTP:  display.print("NTP"); break;
        case T_SYNC_GPS:  display.print("GPS"); break;
        case T_SYNC_APRS: display.print("APR"); break;
        default:          display.print("NO");  break;
    }

    if (config.aprs_beacon > 0)
        snprintf(buf, sizeof(buf), " T%d", config.aprs_beacon);
    else
        strlcpy(buf, " SB", sizeof(buf));
    if (config.tnc)          strlcat(buf, " TN", sizeof(buf));
    if (config.tnc_telemetry)strlcat(buf, " TM", sizeof(buf));
    if (config.tnc_digi)     strlcat(buf, " DG", sizeof(buf));
    if (config.aprs)         strlcat(buf, " IS", sizeof(buf));
    if (config.rf2inet)      strlcat(buf, " RI", sizeof(buf));
    if (config.inet2rf)      strlcat(buf, " IR", sizeof(buf));
    display.setCursor(display.width() - (int16_t)(strlen(buf) * CHAR_WIDTH), 48);
    display.print(buf);

    // ── Row y=56: [stopwatch] age (left) + [|->] distance (right) ──────
    display.drawBitmap(0, 56, icon_clock, 8, 7, WHITE);
    display.setCursor(10, 56);
    if (isValid) {
        display.print(age / 1000);
        display.print("s");
    } else {
        display.print("--s");
    }

    static const char *gpsNames[] = {"AUTO", "GPS", "FIX"};
    const char *modeName = gpsNames[config.gps_mode];
    int16_t modeW = (int16_t)(strlen(modeName) * CHAR_WIDTH);
    display.setCursor(display.width() / 2 - modeW / 2, 56);
    display.print(modeName);

    snprintf(buf, sizeof(buf), "%d", distance);
    int16_t distNumW = (int16_t)(strlen(buf) * CHAR_WIDTH);
    int16_t distIconX = display.width() - distNumW - 10;  // 8px icon + 2px gap
    display.drawBitmap(distIconX, 56, icon_dist, 8, 7, WHITE);
    display.setCursor(distIconX + 10, 56);
    display.print(buf);

    OledDrawMsg();

    display.display();
#endif
}

void OledUpdateFWU() {
#ifdef USE_SCREEN
    if (AFSK_modem->sending) return;

    char buf[24];

    display.clearDisplay();

    // DateTime
    struct tm tmstruct;
    getLocalTime(&tmstruct, 0);
    sprintf(buf, "%02d:%02d:%02d", tmstruct.tm_hour, tmstruct.tm_min, tmstruct.tm_sec);
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2), CHAR_HEIGHT * 2);   // center on the screen
    display.print(buf);

    // Message
    sprintf(buf, "FW Update...");
    display.setCursor((display.width() / 2) - (strlen(buf) * CHAR_WIDTH / 2), display.height() - CHAR_HEIGHT * 3);
    display.print(buf);

    display.display();
#endif
}

// ── Menu drawing ──────────────────────────────────────────────────────────────

static void menuDrawHeader(const char *title) {
    display.fillRect(0, 0, display.width(), CHAR_HEIGHT, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(display.width() / 2 - strlen(title) * CHAR_WIDTH / 2, 0);
    display.print(title);
    display.setTextColor(WHITE, BLACK);
}

static void menuBuildItemLabel(int idx, char *buf, size_t len) {
    static const char *wifiNames[] = {"OFF", "AP", "STA", "AP+STA"};
    static const char *gpsNames[]  = {"AUTO", "GPS", "FIX"};
    switch (idx) {
        case 0: snprintf(buf, len, "Send Beacon");                                     break;
        case 1:
            if (config.aprs_beacon == 0)
                snprintf(buf, len, "Beacon Mode: SB");
            else
                snprintf(buf, len, "Beacon Mode: %dm", config.aprs_beacon);
            break;
        case 2: snprintf(buf, len, "WiFi: %s",    wifiNames[config.wifi_mode & 3]);   break;
        case 3: snprintf(buf, len, "APRS-IS: %s", config.aprs      ? "ON" : "OFF");   break;
        case 4: snprintf(buf, len, "Digi: %s",    config.tnc_digi  ? "ON" : "OFF");   break;
        case 5: snprintf(buf, len, "RF/TNC: %s",  config.tnc       ? "ON" : "OFF");   break;
        case 6: snprintf(buf, len, "RF Pwr: %s",  config.rf_power ? "High" : "Low");   break;
        case 7: snprintf(buf, len, "Send Mode: %s", gpsNames[config.gps_mode]);         break;
        case 8:
            if (config.aprs_ssid > 0)
                snprintf(buf, len, "Call: %s-%d", config.aprs_mycall, config.aprs_ssid);
            else
                snprintf(buf, len, "Call: %s", config.aprs_mycall);
            break;
        case 9:
            snprintf(buf, len, "APRS Symbol: %s", APRS_SYMBOLS[0].name);
            for (int i = 0; i < APRS_SYMBOL_COUNT; i++) {
                if (APRS_SYMBOLS[i].table  == config.aprs_table &&
                    APRS_SYMBOLS[i].symbol == config.aprs_symbol) {
                    snprintf(buf, len, "APRS Symbol: %s", APRS_SYMBOLS[i].name);
                    break;
                }
            }
            break;
        case 10: snprintf(buf, len, "Brightness: %s", BRIGHTNESS_NAMES[brightnessIdx(config.oled_brightness)]); break;
        case 11: {
            if (config.oled_autodim == 0) {
                snprintf(buf, len, "Auto-Dim: OFF");
            } else {
                const char *lvl = (config.oled_autodim_level == 0) ? "OFF" : BRIGHTNESS_NAMES[config.oled_autodim_level - 1];
                snprintf(buf, len, "Auto-Dim: %ds/%s", config.oled_autodim, lvl);
            }
            break;
        }
        case 12:
            if (config.locator_popup == 0)
                snprintf(buf, len, "Locator Popup: OFF");
            else
                snprintf(buf, len, "Locator Popup: %ds", config.locator_popup);
            break;
        case 13:
            if (config.aprs_rx_popup == 0)
                snprintf(buf, len, "APRS RX Popup: OFF");
            else
                snprintf(buf, len, "APRS RX Popup: %ds", config.aprs_rx_popup);
            break;
        case 14: snprintf(buf, len, "RX Packet List");                                   break;
        case 15: {
            int cnt = aprsMsgCount();
            if (cnt > 0) snprintf(buf, len, "MSG Inbox (%d)", cnt);
            else         snprintf(buf, len, "MSG Inbox");
            break;
        }
        case 16: snprintf(buf, len, "GNSS Status");                                      break;
        case 17: snprintf(buf, len, "Battery Status");                                   break;
        case 18: snprintf(buf, len, "About");                                            break;
        case 19: snprintf(buf, len, "Factory Reset!");                                   break;
        default: buf[0] = 0; break;
    }
}

void OledDrawMenu() {
#ifdef USE_SCREEN
    char buf[22];
    display.clearDisplay();
    display.setTextColor(WHITE, BLACK);
    display.setTextSize(1);
    display.setFont();

    switch (menuMode) {

        case MENU_MAIN: {
            menuDrawHeader("  MENU  ");
            for (int i = 0; i < MENU_VISIBLE && (menuScroll + i) < MENU_ITEM_COUNT; i++) {
                int idx = menuScroll + i;
                int y   = CHAR_HEIGHT * (i + 1);
                menuBuildItemLabel(idx, buf, sizeof(buf));
                if (idx == menuSelected) {
                    display.fillRect(0, y, display.width(), CHAR_HEIGHT, WHITE);
                    display.setTextColor(BLACK, WHITE);
                    display.setCursor(2, y);
                    display.print(">");
                    display.setCursor(10, y);
                    display.print(buf);
                    display.setTextColor(WHITE, BLACK);
                } else {
                    display.setCursor(10, y);
                    display.print(buf);
                }
            }
            // scroll indicator bottom row
            if (MENU_ITEM_COUNT > MENU_VISIBLE) {
                snprintf(buf, sizeof(buf), "%d/%d", menuSelected + 1, MENU_ITEM_COUNT);
                display.setCursor(display.width() - strlen(buf) * CHAR_WIDTH, 56);
                display.print(buf);
                if (menuScroll > 0)
                    display.setCursor(0, 56), display.print((char)0x18);  // up arrow
                if (menuScroll + MENU_VISIBLE < MENU_ITEM_COUNT)
                    display.setCursor(CHAR_WIDTH, 56), display.print((char)0x19);  // down arrow
            }
            break;
        }

        case MENU_QUICK: {
            static const char *qLabels[] = {
                "BEACON", "WiFi SW", "RF SW", "SB SW", "RESET", "Cancel"
            };
            const char *label = (menuSubVal < QUICK_ITEM_COUNT) ? qLabels[menuSubVal] : "Cancel";
            display.setTextSize(2);
            int lw = strlen(label) * CHAR_WIDTH * 2;
            display.setCursor((display.width() - lw) / 2, 18);
            display.print(label);
            display.setTextSize(1);
            // phase indicator dots
            for (int i = 0; i < QUICK_ITEM_COUNT; i++) {
                int dx = (display.width() / QUICK_ITEM_COUNT) * i + 4;
                if (i == menuSubVal)
                    display.fillRect(dx, 42, 6, 6, WHITE);
                else
                    display.drawRect(dx, 42, 6, 6, WHITE);
            }
            display.setCursor(10, 56); display.print("release to execute");
            break;
        }

        case MENU_WIFI_MODE: {
            static const char *opts[] = {"OFF", "AP", "STA", "AP+STA"};
            menuDrawHeader(" WiFi Mode ");
            for (int i = 0; i < 4; i++) {
                int y = CHAR_HEIGHT * (i + 1);
                if (i == menuSubVal) {
                    display.fillRect(0, y, display.width(), CHAR_HEIGHT, WHITE);
                    display.setTextColor(BLACK, WHITE);
                    display.setCursor(4, y); display.print("*");
                    display.setCursor(14, y); display.print(opts[i]);
                    display.setTextColor(WHITE, BLACK);
                } else {
                    display.setCursor(14, y); display.print(opts[i]);
                }
            }
            display.setCursor(0, 56); display.print("OK:save  HOLD:cancel");
            break;
        }

        case MENU_GPS_MODE: {
            static const char *opts[] = {"AUTO", "GPS only", "Fixed"};
            menuDrawHeader(" GPS Mode ");
            for (int i = 0; i < 3; i++) {
                int y = CHAR_HEIGHT * (i + 1);
                if (i == menuSubVal) {
                    display.fillRect(0, y, display.width(), CHAR_HEIGHT, WHITE);
                    display.setTextColor(BLACK, WHITE);
                    display.setCursor(4, y); display.print("*");
                    display.setCursor(14, y); display.print(opts[i]);
                    display.setTextColor(WHITE, BLACK);
                } else {
                    display.setCursor(14, y); display.print(opts[i]);
                }
            }
            display.setCursor(0, 56); display.print("OK:save  HOLD:cancel");
            break;
        }

        case MENU_SSID_SEL: {
            menuDrawHeader("Call-SSID");
            char call[7];
            strlcpy(call, menuCallBuf, sizeof(call));
            if (menuSubVal > 0)
                snprintf(buf, sizeof(buf), "%s-< %d >", call, menuSubVal);
            else
                snprintf(buf, sizeof(buf), "%s  < 0 >", call);
            display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, 22);
            display.print(buf);
            display.setCursor(0, 48); display.print("0=no SSID");
            display.setCursor(0, 56); display.print("OK:save  HOLD:cancel");
            break;
        }

        case MENU_CALLSIGN: {
            menuDrawHeader("Call-SSID");
            char disp[7];
            for (int i = 0; i < 6; i++)
                disp[i] = (menuCallBuf[i] != 0) ? menuCallBuf[i] : '_';
            disp[6] = 0;
            // show SSID suffix only if non-zero
            if (menuSubVal > 0)
                snprintf(buf, sizeof(buf), "%s-%d", disp, menuSubVal);
            else
                snprintf(buf, sizeof(buf), "%s", disp);
            int cx = display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2;
            display.setCursor(cx, 16);
            display.print(buf);
            display.setCursor(cx + menuCallPos * CHAR_WIDTH, 25);
            display.print("^");
            display.setCursor(0, 40); display.print("rot:char  OK:next");
            display.setCursor(0, 48); display.print("min 3  HOLD:cancel");
            break;
        }

        case MENU_SYMBOL: {
            menuDrawHeader("APRS Symbol");
            const AprsSymbol &sym = APRS_SYMBOLS[menuSubVal];
            // Icon shifted up 1px to compensate for blank top padding in bitmaps
            display.drawBitmap(2, 12,
                (const uint8_t *)pgm_read_ptr(&aprs_symbol_bitmaps[menuSubVal]),
                16, 16, WHITE);
            display.setFont(&FreeSans9pt7b);
            display.setCursor(22, 25);
            display.print(sym.name);
            display.setFont();
            snprintf(buf, sizeof(buf), "%c%c  (%d/%d)", sym.table, sym.symbol,
                     menuSubVal + 1, APRS_SYMBOL_COUNT);
            display.setCursor((display.width() - strlen(buf) * CHAR_WIDTH) / 2, 34);
            display.print(buf);
            display.setCursor(0, 48); display.print("rot:sel  OK:save");
            break;
        }

        case MENU_BEACON: {
            menuDrawHeader("Beacon Mode");
            char valBuf[8];
            if (menuSubVal == 0)
                snprintf(valBuf, sizeof(valBuf), "SB");
            else if (BEACON_INTERVALS[menuSubVal] == 1)
                snprintf(valBuf, sizeof(valBuf), "1 min");
            else
                snprintf(valBuf, sizeof(valBuf), "%d min", BEACON_INTERVALS[menuSubVal]);
            int vw = strlen(valBuf) * CHAR_WIDTH * 2;
            display.setTextSize(2);
            display.setCursor((display.width() - vw) / 2, 24);
            display.print(valBuf);
            display.setTextSize(1);
            display.setCursor(0, 56); display.print("rot:change  OK:save");
            break;
        }

        case MENU_BRIGHTNESS: {
            menuDrawHeader("Brightness");
            // 5 filled blocks representing level
            int bw = 18, bh = 16, gap = 4;
            int totalW = BRIGHTNESS_LEVEL_COUNT * bw + (BRIGHTNESS_LEVEL_COUNT - 1) * gap;
            int startX = (display.width() - totalW) / 2;
            for (int i = 0; i < BRIGHTNESS_LEVEL_COUNT; i++) {
                int bx = startX + i * (bw + gap);
                if (i <= menuSubVal)
                    display.fillRect(bx, 20, bw, bh, WHITE);
                else
                    display.drawRect(bx, 20, bw, bh, WHITE);
            }
            snprintf(buf, sizeof(buf), "%s", BRIGHTNESS_NAMES[menuSubVal]);
            display.setCursor(display.width() / 2 - strlen(buf) * CHAR_WIDTH / 2, 42);
            display.print(buf);
            display.setCursor(0, 56); display.print("OK:save  HOLD:cancel");
            break;
        }

        case MENU_AUTODIM: {
            char valBuf[8];
            if (menuAutodimPhase == 0) {
                menuDrawHeader("Auto-Dim");
                if (menuSubVal == 0) snprintf(valBuf, sizeof(valBuf), "OFF");
                else                 snprintf(valBuf, sizeof(valBuf), "%d s", menuSubVal);
                int adw = strlen(valBuf) * CHAR_WIDTH * 2;
                display.setTextSize(2);
                display.setCursor((display.width() - adw) / 2, 24);
                display.print(valBuf);
                display.setTextSize(1);
                const char *lvlHint = (config.oled_autodim_level == 0) ? "OFF" : BRIGHTNESS_NAMES[config.oled_autodim_level - 1];
                char hintBuf[22];
                snprintf(hintBuf, sizeof(hintBuf), "Level: %-8s OK:next", lvlHint);
                display.setCursor(0, 56); display.print(hintBuf);
            } else {
                menuDrawHeader("Dim Level");
                const char *lvlName = (menuSubVal == 0) ? "OFF" : BRIGHTNESS_NAMES[menuSubVal - 1];
                int alw = strlen(lvlName) * CHAR_WIDTH * 2;
                display.setTextSize(2);
                display.setCursor((display.width() - alw) / 2, 24);
                display.print(lvlName);
                display.setTextSize(1);
                char hintBuf[22];
                if (menuAutodimTimeout == 0) snprintf(hintBuf, sizeof(hintBuf), "Tout: OFF     OK:save");
                else                         snprintf(hintBuf, sizeof(hintBuf), "Tout: %-3ds    OK:save", menuAutodimTimeout);
                display.setCursor(0, 56); display.print(hintBuf);
            }
            break;
        }

        case MENU_LOCATOR_POP: {
            menuDrawHeader("Locator Pop");
            char valBuf[8];
            if (menuSubVal == 0)
                snprintf(valBuf, sizeof(valBuf), "OFF");
            else
                snprintf(valBuf, sizeof(valBuf), "%d s", menuSubVal);
            int lw = strlen(valBuf) * CHAR_WIDTH * 2;
            display.setTextSize(2);
            display.setCursor((display.width() - lw) / 2, 24);
            display.print(valBuf);
            display.setTextSize(1);
            display.setCursor(0, 56); display.print("rot:change  OK:save");
            break;
        }

        case MENU_RX_POPUP: {
            menuDrawHeader("APRS RX Popup");
            char valBuf[8];
            if (menuSubVal == 0)
                snprintf(valBuf, sizeof(valBuf), "OFF");
            else
                snprintf(valBuf, sizeof(valBuf), "%d s", menuSubVal);
            int rw = strlen(valBuf) * CHAR_WIDTH * 2;
            display.setTextSize(2);
            display.setCursor((display.width() - rw) / 2, 24);
            display.print(valBuf);
            display.setTextSize(1);
            display.setCursor(0, 56); display.print("rot:change  OK:save");
            break;
        }

        case MENU_GNSS: {
            menuDrawHeader(" GNSS STATUS ");
            bool fix = gps.location.isValid();

            // Per-constellation satellite counts (always shown)
            snprintf(buf, sizeof(buf), "GPS:%-2d BDS:%-2d GLO:%-2d",
                     gnssSatsGPS, gnssSatsBDS, gnssSatsGLO);
            display.setCursor(0, 8); display.print(buf);

            // Lat / Lon (--- when no fix)
            display.setCursor(0, 16);
            display.print("Lat: ");
            display.print(fix ? deg_to_nmea(lat, true) : "--------");

            display.setCursor(0, 24);
            display.print("Lon: ");
            display.print(fix ? deg_to_nmea(lon, false) : "---------");

            // Altitude + fix dimensionality
            if (fix) {
                const char *ftype = gps.altitude.isValid() ? "3D" : "2D";
                snprintf(buf, sizeof(buf), "Alt:%.1fm  %s", gps.altitude.meters(), ftype);
            } else {
                snprintf(buf, sizeof(buf), "Alt: ---");
            }
            display.setCursor(0, 32); display.print(buf);

            // Speed + Course
            snprintf(buf, sizeof(buf), "Spd:%.1f  Crs:%.0f",
                     fix ? gps.speed.kmph() : 0.0f,
                     fix ? gps.course.deg() : 0.0f);
            display.setCursor(0, 40); display.print(buf);

            // HDOP + total sats + rolling packet counter
            if (gps.hdop.isValid())
                snprintf(buf, sizeof(buf), "HDOP:%.1f Sat:%-2d #%02lu",
                         gps.hdop.hdop(), (int)gps.satellites.value(), GpsPktCnt() % 100);
            else
                snprintf(buf, sizeof(buf), "HDOP:--- Sat:-- #%02lu", GpsPktCnt() % 100);
            display.setCursor(0, 48); display.print(buf);

            display.setCursor(0, 56); display.print("press: back");
            break;
        }

        case MENU_BATTERY: {
            menuDrawHeader("BATTERY STATUS");
#ifdef USE_PMU
            const char *state;
            if (!PMU.isBatteryConnect()) {
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
            snprintf(buf, sizeof(buf), "State: %s", state);
            display.setCursor(0,  8); display.print(buf);
            snprintf(buf, sizeof(buf), "Bat:%4dmV %3d%%",
                     (int)PMU.getBattVoltage(),
                     PMU.isBatteryConnect() ? (int)PMU.getBatteryPercent() : 0);
            display.setCursor(0, 16); display.print(buf);
            if (vbusIn)
                snprintf(buf, sizeof(buf), "VBUS:%4dmV", (int)PMU.getVbusVoltage());
            else
                snprintf(buf, sizeof(buf), "VBUS: ---");
            display.setCursor(0, 24); display.print(buf);
            snprintf(buf, sizeof(buf), "Sys: %4dmV", (int)PMU.getSystemVoltage());
            display.setCursor(0, 32); display.print(buf);
            snprintf(buf, sizeof(buf), "PMU: %.1f C", PMU.getTemperature());
            display.setCursor(0, 40); display.print(buf);
            snprintf(buf, sizeof(buf), "Bat: %s", PMU.isBatteryConnect() ? "present" : "absent");
            display.setCursor(0, 48); display.print(buf);
#else
            display.setCursor(0, 24); display.print("No PMU hardware");
#endif
            display.setCursor(0, 56); display.print("press: back");
            break;
        }

        case MENU_ABOUT: {
            menuDrawHeader("  About  ");
            snprintf(buf, sizeof(buf), "APRS-ESP v%s", VERSION_FULL);
            display.setCursor(0,  9); display.print(buf);
            display.setCursor(0, 17); display.print("by LY3PH Ernest");
            if (config.aprs_ssid > 0)
                snprintf(buf, sizeof(buf), "%s-%d", config.aprs_mycall, config.aprs_ssid);
            else
                snprintf(buf, sizeof(buf), "%s", config.aprs_mycall);
            display.setCursor(0, 25); display.print(buf);
            display.setCursor(0, 33); display.print("github.com/erstec");
            display.setCursor(0, 41);
            display.print("Up:");
            {
                char uptBuf[16];
                int s = (int)(millis() / 1000);
                snprintf(uptBuf, sizeof(uptBuf), "%dd %02d:%02d:%02d",
                         s / 86400, (s % 86400) / 3600,
                         (s % 3600) / 60, s % 60);
                display.print(uptBuf);
            }
            display.setCursor(0, 49);
            if (config.wifi_mode == WIFI_STA_FIX) {
                display.print(WiFi.localIP());
            } else if (config.wifi_mode == WIFI_AP_STA_FIX || config.wifi_mode == WIFI_AP_FIX) {
                if (WiFi.localIP() != IPAddress(0, 0, 0, 0))
                    display.print(WiFi.localIP());
                else
                    display.print(WiFi.softAPIP());
            } else {
                display.print("WiFi: OFF");
            }
            display.setCursor(0, 56); display.print("press: back");
            break;
        }

        case MENU_CONFIRM_RESET: {
            menuDrawHeader("Factory Reset!");
            display.setCursor(0, 10); display.print("Erase all settings?");
            // NO / YES buttons
            int noX  = 16;
            int yesX = 76;
            int btnY = 30;
            if (menuSubVal == 0) {
                display.fillRect(noX - 2, btnY - 1, 36, CHAR_HEIGHT + 2, WHITE);
                display.setTextColor(BLACK, WHITE);
                display.setCursor(noX, btnY); display.print("[ NO ]");
                display.setTextColor(WHITE, BLACK);
                display.setCursor(yesX, btnY); display.print("[ YES ]");
            } else {
                display.setCursor(noX, btnY); display.print("[ NO ]");
                display.fillRect(yesX - 2, btnY - 1, 42, CHAR_HEIGHT + 2, WHITE);
                display.setTextColor(BLACK, WHITE);
                display.setCursor(yesX, btnY); display.print("[ YES ]");
                display.setTextColor(WHITE, BLACK);
            }
            display.setCursor(0, 56); display.print("rot:toggle  OK:exec");
            break;
        }

        case MENU_WIFI_REBOOT_CONFIRM: {
            menuDrawHeader("WiFi Changed");
            display.setCursor(0, 10); display.print("Reboot to apply?");
            int yesX = 16, noX = 76, btnY = 30;
            if (menuSubVal == 0) {  // YES selected (default)
                display.fillRect(yesX - 2, btnY - 1, 42, CHAR_HEIGHT + 2, WHITE);
                display.setTextColor(BLACK, WHITE);
                display.setCursor(yesX, btnY); display.print("[ YES ]");
                display.setTextColor(WHITE, BLACK);
                display.setCursor(noX, btnY); display.print("[ NO ]");
            } else {  // NO selected
                display.setCursor(yesX, btnY); display.print("[ YES ]");
                display.fillRect(noX - 2, btnY - 1, 36, CHAR_HEIGHT + 2, WHITE);
                display.setTextColor(BLACK, WHITE);
                display.setCursor(noX, btnY); display.print("[ NO ]");
                display.setTextColor(WHITE, BLACK);
            }
            display.setCursor(0, 56); display.print("rot:toggle  OK:exec");
            break;
        }

        case MENU_RX_LIST: {
            menuDrawHeader(" RX PACKET LIST");
            if (rxListCount == 0) {
                display.setCursor(16, 28); display.print("No packets yet");
            } else {
                int scroll = (menuSubVal >= MENU_VISIBLE) ? menuSubVal - MENU_VISIBLE + 1 : 0;
                for (int i = 0; i < MENU_VISIBLE && (scroll + i) < rxListCount; i++) {
                    int idx = scroll + i;
                    pkgListType e = getPkgList(idx);
                    // type abbreviation
                    const char *typ;
                    if      (e.type & FILTER_MESSAGE)    typ = "Msg";
                    else if (e.type & FILTER_WX)         typ = "WX ";
                    else if (e.type & FILTER_OBJECT)     typ = "Obj";
                    else if (e.type & FILTER_ITEM)       typ = "Itm";
                    else if (e.type & FILTER_TELEMETRY)  typ = "Tlm";
                    else if (e.type & FILTER_STATUS)     typ = "Sts";
                    else if (e.type & FILTER_THIRDPARTY) typ = "3rd";
                    else if (e.type & FILTER_MICE)       typ = "MiE";
                    else if (e.type & FILTER_POSITION)   typ = "Pos";
                    else                                  typ = "???";
                    // age
                    char age[6] = "---";
                    if (e.time > 0) {
                        time_t secs = time(NULL) - e.time;
                        if (secs < 0)        secs = 0;
                        if (secs < 60)       snprintf(age, sizeof(age), "%lds",  (long)secs);
                        else if (secs < 3600) snprintf(age, sizeof(age), "%ldm", (long)(secs / 60));
                        else if (secs < 86400) snprintf(age, sizeof(age), "%ldh", (long)(secs / 3600));
                        else                  snprintf(age, sizeof(age), "%ldd", (long)(secs / 86400));
                    }
                    snprintf(buf, sizeof(buf), "%-9.9s %s %s %-4s",
                             e.calsign, typ, e.channel ? "IS" : "RF", age);
                    int y = CHAR_HEIGHT * (1 + i);
                    if (idx == menuSubVal) {
                        display.fillRect(0, y, OLED_WIDTH, CHAR_HEIGHT, WHITE);
                        display.setTextColor(BLACK, WHITE);
                        display.setCursor(0, y); display.print(buf);
                        display.setTextColor(WHITE, BLACK);
                    } else {
                        display.setCursor(0, y); display.print(buf);
                    }
                }
            }
            display.setCursor(0, 56); display.print("OK:detail  HOLD:exit");
            break;
        }

        case MENU_RX_DETAIL: {
            if (rxListCount == 0 || menuSubVal >= rxListCount) {
                menuDrawHeader("RX DETAIL");
                display.setCursor(16, 28); display.print("No entry");
                display.setCursor(0, 56); display.print("OK:back");
                break;
            }
            pkgListType e = getPkgList(menuSubVal);
            // header = callsign
            char hdr[17];
            snprintf(hdr, sizeof(hdr), " %.15s", e.calsign);
            menuDrawHeader(hdr);
            // row 1: type + channel + count
            const char *typ;
            if      (e.type & FILTER_MESSAGE)    typ = "Msg";
            else if (e.type & FILTER_WX)         typ = "WX ";
            else if (e.type & FILTER_OBJECT)     typ = "Obj";
            else if (e.type & FILTER_ITEM)       typ = "Itm";
            else if (e.type & FILTER_TELEMETRY)  typ = "Tlm";
            else if (e.type & FILTER_STATUS)     typ = "Sts";
            else if (e.type & FILTER_THIRDPARTY) typ = "3rd";
            else if (e.type & FILTER_MICE)       typ = "MiE";
            else if (e.type & FILTER_POSITION)   typ = "Pos";
            else                                  typ = "???";
            snprintf(buf, sizeof(buf), "%s %s %u pkts",
                     typ, e.channel ? "IS" : "RF", e.pkg);
            display.setCursor(0,  8); display.print(buf);
            // row 2: age
            char age[12] = "no time";
            if (e.time > 0) {
                time_t secs = time(NULL) - e.time;
                if (secs < 0) secs = 0;
                if (secs < 60)        snprintf(age, sizeof(age), "%lds ago",  (long)secs);
                else if (secs < 3600)  snprintf(age, sizeof(age), "%ldm ago", (long)(secs / 60));
                else if (secs < 86400) snprintf(age, sizeof(age), "%ldh ago", (long)(secs / 3600));
                else                   snprintf(age, sizeof(age), "%ldd ago", (long)(secs / 86400));
            }
            display.setCursor(0, 16); display.print(age);
            // rows 3-6: raw packet content (4 × 21 chars)
            for (int r = 0; r < 4; r++) {
                int off = r * 21;
                if (e.raw[off] == '\0') break;
                char line[22];
                snprintf(line, sizeof(line), "%.21s", e.raw + off);
                display.setCursor(0, 24 + r * CHAR_HEIGHT); display.print(line);
            }
            display.setCursor(0, 56); display.print("OK:back rot:nav");
            break;
        }

        case MENU_MSG_INBOX: {
            menuDrawHeader(" MSG INBOX ");
            int cnt = aprsMsgCount();
            if (cnt == 0) {
                display.setCursor(16, 28); display.print("No messages");
            } else {
                int scroll = (menuSubVal >= MENU_VISIBLE) ? menuSubVal - MENU_VISIBLE + 1 : 0;
                for (int i = 0; i < MENU_VISIBLE && (scroll + i) < cnt; i++) {
                    int idx = scroll + i;
                    AprsMsg m = aprsMsgGet(idx);
                    char age[6] = "---";
                    if (m.when > 0) {
                        time_t secs = time(NULL) - m.when;
                        if (secs < 60)         snprintf(age, sizeof(age), "%lds",  (long)secs);
                        else if (secs < 3600)  snprintf(age, sizeof(age), "%ldm", (long)(secs / 60));
                        else if (secs < 86400) snprintf(age, sizeof(age), "%ldh", (long)(secs / 3600));
                        else                   snprintf(age, sizeof(age), "%ldd", (long)(secs / 86400));
                    }
                    snprintf(buf, sizeof(buf), "%-9.9s %-4s", m.from, age);
                    int y = CHAR_HEIGHT * (1 + i);
                    if (idx == menuSubVal) {
                        display.fillRect(0, y, OLED_WIDTH, CHAR_HEIGHT, WHITE);
                        display.setTextColor(BLACK, WHITE);
                        display.setCursor(0, y); display.print(buf);
                        display.setTextColor(WHITE, BLACK);
                    } else {
                        display.setCursor(0, y); display.print(buf);
                    }
                }
            }
            display.setCursor(0, 56); display.print("OK:read   HOLD:exit");
            break;
        }

        case MENU_MSG_DETAIL: {
            int cnt = aprsMsgCount();
            if (cnt == 0 || menuSubVal >= cnt) {
                menuDrawHeader("MSG DETAIL");
                display.setCursor(16, 28); display.print("No entry");
                display.setCursor(0, 56); display.print("OK:back");
                break;
            }
            AprsMsg m = aprsMsgGet(menuSubVal);
            char hdr[17];
            snprintf(hdr, sizeof(hdr), " %.15s", m.from);
            menuDrawHeader(hdr);
            char age[16] = "no time";
            if (m.when > 0) {
                time_t secs = time(NULL) - m.when;
                if (secs < 0) secs = 0;
                if (secs < 60)         snprintf(age, sizeof(age), "%lds ago",  (long)secs);
                else if (secs < 3600)  snprintf(age, sizeof(age), "%ldm ago", (long)(secs / 60));
                else if (secs < 86400) snprintf(age, sizeof(age), "%ldh ago", (long)(secs / 3600));
                else                   snprintf(age, sizeof(age), "%ldd ago", (long)(secs / 86400));
            }
            display.setCursor(0, 8); display.print(age);
            for (int r = 0; r < 4; r++) {
                int off = r * 21;
                if (m.text[off] == '\0') break;
                char line[22];
                snprintf(line, sizeof(line), "%.21s", m.text + off);
                display.setCursor(0, 16 + r * CHAR_HEIGHT); display.print(line);
            }
            display.setCursor(0, 56); display.print("OK:back rot:nav");
            break;
        }

        default: break;
    }

    display.display();
#endif
}

const uint8_t BRIGHTNESS_LEVELS[BRIGHTNESS_LEVEL_COUNT] = {25, 75, 128, 200, 255};
const char * const BRIGHTNESS_NAMES[BRIGHTNESS_LEVEL_COUNT] = {"Min", "Low", "Medium", "High", "Max"};

uint8_t brightnessIdx(uint8_t val) {
    uint8_t best = 0, bestDiff = 255;
    for (uint8_t i = 0; i < BRIGHTNESS_LEVEL_COUNT; i++) {
        uint8_t d = (uint8_t)abs((int)val - (int)BRIGHTNESS_LEVELS[i]);
        if (d < bestDiff) { bestDiff = d; best = i; }
    }
    return best;
}



void OledSetBrightness(uint8_t val) {
#if defined(USE_SCREEN_SSD1306)
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(val);
#elif defined(USE_SCREEN_SH1106)
    display.sh1106_command(0x81);
    display.sh1106_command(val);
#endif
}

bool OledIsOn() { return oledOn; }

void OledTogglePower() {
    oledOn = !oledOn;
#if defined(USE_SCREEN_SSD1306)
    display.ssd1306_command(oledOn ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
#elif defined(USE_SCREEN_SH1106)
    display.sh1106_command(oledOn ? SH1106_DISPLAYON : SH1106_DISPLAYOFF);
#endif
}

extern volatile bool menuUpdateNeeded;
extern uint8_t       menuAutodimPhase;
extern uint8_t       menuAutodimTimeout;

bool OledIsPopupActive() {
    // flush expired head entries first
    while (popupCount > 0 && popupCurrent()->timeout == 0) {
        popupHead = (popupHead + 1) % POPUP_QUEUE_SIZE;
        popupCount--;
    }
    return popupCount > 0;
}

void OledDismissPopup() {
    if (popupCount > 0) {
        popupHead = (popupHead + 1) % POPUP_QUEUE_SIZE;
        popupCount--;
    }
    oledActivityMs = millis();
}

void OledResetActivity() {
#ifdef USE_SCREEN
    if (oledDimmed) {
        oledDimmed = false;
        oledOn = true;  // re-sync toggle state when undimming
#if defined(USE_SCREEN_SSD1306)
        display.ssd1306_command(SSD1306_DISPLAYON);
#elif defined(USE_SCREEN_SH1106)
        display.sh1106_command(SH1106_DISPLAYON);
#endif
        OledSetBrightness(config.oled_brightness);
        menuUpdateNeeded = true;
    }
#endif
    oledActivityMs = millis();
}

void OledCheckAutoDim() {
#ifdef USE_SCREEN
    if (!oledOn || config.oled_autodim == 0 || oledDimmed) return;
    if (millis() - oledActivityMs < (unsigned long)config.oled_autodim * 1000UL) return;
    oledDimmed = true;
    if (config.oled_autodim_level == 0) {
#if defined(USE_SCREEN_SSD1306)
        display.ssd1306_command(SSD1306_DISPLAYOFF);
#elif defined(USE_SCREEN_SH1106)
        display.sh1106_command(SH1106_DISPLAYOFF);
#endif
    } else {
        OledSetBrightness(BRIGHTNESS_LEVELS[config.oled_autodim_level - 1]);
    }
#endif
}

// add message to show in rectangle box for timeout ticks; queued
void OledPushMsg(String caption, char *msg, char *msg2, uint8_t timeout) {
#ifdef USE_SCREEN
    if (!oledOn) return;  // screen off — drop transient notification
#endif
    OledResetActivity();
    PopupEntry e = {};
    strncpy(e.caption, caption.c_str(), sizeof(e.caption) - 1);
    strncpy(e.msg,  msg  ? msg  : "", sizeof(e.msg)  - 1);
    strncpy(e.msg2, msg2 ? msg2 : "", sizeof(e.msg2) - 1);
    e.timeout  = timeout;
    e.bar      = -1;
    e.dualFont = false;
    popupPush(e);
#ifdef USE_SCREEN
    OledDrawMsg(false);
    display.display();
#endif
}

void OledPushMsgDual(const char *line1, const char *line2, uint8_t timeout) {
#ifdef USE_SCREEN
    if (!oledOn) return;  // screen off — drop transient notification
#endif
    OledResetActivity();
    PopupEntry e = {};
    strncpy(e.msg,  line1 ? line1 : "", sizeof(e.msg)  - 1);
    strncpy(e.msg2, line2 ? line2 : "", sizeof(e.msg2) - 1);
    e.timeout  = timeout;
    e.bar      = -1;
    e.dualFont = true;
    popupPush(e);
#ifdef USE_SCREEN
    OledDrawMsg(false);
    display.display();
#endif
}

void OledPushMsgBar(const char *caption, char *msg, char *msg2, int8_t bar, uint8_t timeout) {
#ifdef USE_SCREEN
    if (!oledOn) return;  // screen off — drop transient notification
#endif
    OledResetActivity();
    PopupEntry e = {};
    strncpy(e.caption, caption, sizeof(e.caption) - 1);
    strncpy(e.msg,  msg  ? msg  : "", sizeof(e.msg)  - 1);
    strncpy(e.msg2, msg2 ? msg2 : "", sizeof(e.msg2) - 1);
    e.timeout  = timeout;
    e.bar      = bar;
    e.dualFont = false;
    popupPush(e);
#ifdef USE_SCREEN
    OledDrawMsg(false);
    display.display();
#endif
}

// Enqueue a popup behind any existing ones — used for APRS messages that must not be lost
void OledEnqueueMsg(String caption, char *msg, char *msg2, uint8_t timeout) {
    OledResetActivity();
    PopupEntry e = {};
    strncpy(e.caption, caption.c_str(), sizeof(e.caption) - 1);
    strncpy(e.msg,  msg  ? msg  : "", sizeof(e.msg)  - 1);
    strncpy(e.msg2, msg2 ? msg2 : "", sizeof(e.msg2) - 1);
    e.timeout  = timeout;
    e.bar      = -1;
    e.dualFont = false;
    popupPush(e, true);  // enqueue=true: never drops, always queued
#ifdef USE_SCREEN
    if (oledOn) {  // skip draw/flush when screen is off; popup shows on next OledUpdate
        OledDrawMsg(false);
        display.display();
    }
#endif
}
