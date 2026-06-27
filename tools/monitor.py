import threading
import queue
import json
import time
import os
import serial
import serial.tools.list_ports
import dearpygui.dearpygui as dpg

W = 1100
H = 740
VERSION = "2.0"
MAX_LOG = 300

TAG_COLORS = {
    "[DEBUG]":  (100, 210, 100, 255),
    "[STATUS]": (100, 160, 255, 255),
    "[SB]":     (220, 180, 60,  255),
    "[TX]":     (255, 120, 80,  255),
    "[BTN]":    (200, 120, 255, 255),
    "[MSG]":    (255, 200, 80,  255),
    "[APRS]":   (120, 220, 220, 255),
    "[PKT]":    (180, 180, 255, 255),
}
COL_PLAIN = (160, 160, 160, 255)
COL_LABEL = (130, 130, 130, 255)
COL_HEAD  = (100, 170, 255, 255)
COL_OK    = (80,  210,  80, 255)
COL_ERR   = (210,  80,  80, 255)
COL_WARN  = (220, 180,  60, 255)
COL_TX    = (255, 100,  60, 255)

TAGS = list(TAG_COLORS.keys())

event_queue  = queue.Queue()
serial_port  = None
stop_event   = threading.Event()
log_file     = None
log_enabled  = False

state = {
    "connected": False,
    "sys":  {"t": "---", "uptime": 0, "heap": 0, "cpu": 0.0, "rssi": 0, "ip": "---"},
    "gps":  {"online": 0, "fix": 0, "lat": 0.0, "lon": 0.0, "alt": 0.0,
             "spd": 0.0, "hdg": 0.0, "sats": 0, "hdop": 0.0,
             "gps_n": 0, "bds_n": 0, "glo_n": 0, "age": 0},
    "bat":  {"pct": 0, "mv": 0, "vbus": 0, "sys_mv": 0, "chg": "---", "temp": 0.0},
    "aprs": {"all": 0, "drop": 0, "rf2in": 0, "in2rf": 0},
    "cfg":  {"call": "---", "ssid": 0, "tnc": 0, "digi": 0, "aprs_is": 0,
             "wifi": 0, "sb": 0, "bcn": 0, "gps_mode": 0,
             "rf2inet": 0, "inet2rf": 0,
             "path": "---", "symbol": "---",
             "freq_rx": 0.0, "freq_tx": 0.0,
             "pwr": 0, "sql": 0, "vol": 0},
    "sb":   {"dist_m": 0, "spd_kmh": 0, "elapsed_s": 0, "rate_s": 1200,
             "will_tx": 0, "turn_deg": 0, "thr_deg": 0, "result": "---"},
    "rf":   {"ptt": False,
             "last_tx": "---", "tx_t": "",
             "last_rx": "---", "rx_t": "",
             "last_sms": "---", "sms_t": ""},
}


def uptime_str(s):
    h, r = divmod(int(s), 3600)
    m, sec = divmod(r, 60)
    return f"{h:02d}:{m:02d}:{sec:02d}"

def wifi_str(v):
    return {0: "OFF", 1: "AP", 2: "STA", 3: "AP+STA"}.get(int(v), str(v))

def parse_line(line):
    for tag in TAGS:
        if tag in line:
            try:
                idx = line.index(tag) + len(tag)
                data = json.loads(line[idx:].strip())
                return tag, data
            except Exception:
                pass
    return None, None


def apply_status(d):
    s = state["sys"]
    s["t"]      = d.get("t", "---")
    s["uptime"] = d.get("uptime", 0)
    s["heap"]   = d.get("heap", 0)
    s["cpu"]    = d.get("cpu_temp", 0.0)
    s["rssi"]   = d.get("rssi", 0)
    ip = d.get("ip", "---")
    s["ip"] = "---" if ip in ("0.0.0.0", "", "---") else ip

    g  = d.get("gps", {})
    sg = state["gps"]
    sg["online"] = g.get("online", 0)
    sg["fix"]    = g.get("fix", 0)
    if sg["fix"]:
        sg["lat"] = g.get("lat", 0.0)
        sg["lon"] = g.get("lon", 0.0)
        sg["alt"] = g.get("alt", 0.0)
        sg["spd"] = g.get("spd", 0.0)
        sg["hdg"] = g.get("hdg", 0.0)
        sg["age"] = g.get("age", 0)
    sg["sats"]  = g.get("sats", 0)
    sg["hdop"]  = g.get("hdop", 0.0)
    sg["gps_n"] = g.get("gps_n", 0)
    sg["bds_n"] = g.get("bds_n", 0)
    sg["glo_n"] = g.get("glo_n", 0)

    b  = d.get("bat", {})
    sb = state["bat"]
    sb["pct"]    = b.get("pct", -1)
    sb["mv"]     = b.get("mv", 0)
    sb["vbus"]   = b.get("vbus", 0)
    sb["sys_mv"] = b.get("sys_mv", 0)
    sb["chg"]    = b.get("chg", "---")
    sb["temp"]   = b.get("temp", 0.0)

    a  = d.get("aprs", {})
    sa = state["aprs"]
    sa["all"]   = a.get("all", 0)
    sa["drop"]  = a.get("drop", 0)
    sa["rf2in"] = a.get("rf2in", 0)
    sa["in2rf"] = a.get("in2rf", 0)

    c  = d.get("cfg", {})
    sc = state["cfg"]
    sc["call"]     = c.get("call", "---")
    sc["ssid"]     = c.get("ssid", 0)
    sc["tnc"]      = c.get("tnc", 0)
    sc["digi"]     = c.get("digi", 0)
    sc["aprs_is"]  = c.get("aprs", 0)
    sc["wifi"]     = c.get("wifi", 0)
    sc["sb"]       = c.get("sb", 0)
    sc["bcn"]      = c.get("bcn", 0)
    sc["gps_mode"] = c.get("gps_mode", 0)
    sc["rf2inet"]  = c.get("rf2inet", 0)
    sc["inet2rf"]  = c.get("inet2rf", 0)
    sc["path"]     = c.get("path", "---")
    sc["symbol"]   = c.get("symbol", "---")
    sc["freq_rx"]  = c.get("freq_rx", 0.0)
    sc["freq_tx"]  = c.get("freq_tx", 0.0)
    sc["pwr"]      = c.get("pwr", 0)
    sc["sql"]      = c.get("sql", 0)
    sc["vol"]      = c.get("vol", 0)

def apply_debug(d):
    sg = state["gps"]
    sg["fix"]    = d.get("fix", 0)
    sg["online"] = d.get("gps", 0)
    if sg["fix"]:
        sg["lat"] = d.get("lat", 0) / 1e6
        sg["lon"] = d.get("lon", 0) / 1e6
        sg["age"] = d.get("age", 0)
    state["bat"]["pct"] = d.get("bat", -1)

def apply_sb(d):
    ss = state["sb"]
    fn = d.get("fn", "")
    if fn == "check":
        ss["dist_m"]    = d.get("dist_m", 0)
        ss["spd_kmh"]   = d.get("spd_kmh", 0)
        ss["elapsed_s"] = d.get("elapsed_s", 0)
        ss["rate_s"]    = d.get("rate_s", 1200)
        ss["will_tx"]   = d.get("will_tx", 0)
    elif fn == "corner":
        ss["turn_deg"] = d.get("turn_deg", 0)
        ss["thr_deg"]  = d.get("thr_deg", 0)
        ss["result"]   = d.get("result", "---")

def apply_tx(d):
    sr = state["rf"]
    ev = d.get("event", "")
    if ev == "ptt_release":
        sr["ptt"] = False
    elif ev == "send":
        sr["ptt"] = True
    if "pkt" in d and d.get("dir", "") == "RF":
        sr["ptt"]     = True
        sr["last_tx"] = d["pkt"]
        sr["tx_t"]    = time.strftime("%H:%M:%S")

def apply_pkt(d):
    sr  = state["rf"]
    direction = d.get("dir", "")
    pkt = d.get("pkt", "")
    if pkt and direction in ("inet2rf", "rf_rx"):
        # Skip message packets addressed specifically to us — shown in Last SMS via [MSG]
        our_call = state["cfg"]["call"]
        ssid     = state["cfg"]["ssid"]
        our_addr = f"{our_call}-{ssid}" if ssid > 0 else our_call
        if "::" in pkt and our_addr not in ("---", "") and f"::{our_addr}" in pkt:
            return
        sr["last_rx"] = pkt
        sr["rx_t"]    = time.strftime("%H:%M:%S")

def apply_msg(d):
    sr = state["rf"]
    ev = d.get("event", "")
    if ev == "rx":
        fr   = d.get("from", "?")
        text = d.get("text", "")
        sr["last_sms"] = f"{fr}: {text}"
        sr["sms_t"]    = time.strftime("%H:%M:%S")


def serial_reader(port, baud):
    global serial_port
    try:
        serial_port = serial.Serial(port, baud, timeout=1)
        event_queue.put(("__connected__", None, ""))
        while not stop_event.is_set():
            try:
                raw = serial_port.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").rstrip()
                    tag, data = parse_line(line)
                    event_queue.put((tag, data, line))
            except serial.SerialException:
                break
    except Exception as e:
        event_queue.put(("__error__", None, str(e)))
    finally:
        try:
            if serial_port and serial_port.is_open:
                serial_port.close()
        except Exception:
            pass
        event_queue.put(("__disconnected__", None, ""))


def connect_toggle_cb():
    global stop_event
    if state["connected"]:
        dpg.configure_item("btn_connect", label="Disconnecting…", enabled=False)
        stop_event.set()
        if serial_port:
            try:
                serial_port.close()
            except Exception:
                pass
    else:
        port = dpg.get_value("port_combo")
        baud = int(dpg.get_value("baud_combo"))
        dpg.configure_item("btn_connect", label="Connecting…", enabled=False)
        stop_event = threading.Event()
        threading.Thread(target=serial_reader, args=(port, baud), daemon=True).start()

ESP_VIDS = {"303a", "10c4", "1a86", "0403"}
ESP_KW   = ("esp", "cp210", "ch340", "ch341", "ftdi", "uart", "usbserial", "usbmodem", "slab")

def rank_port(p):
    vid = (p.vid or 0)
    score = 0
    if f"{vid:04x}" in ESP_VIDS:       score += 10
    desc = (p.description or "").lower()
    if any(k in desc for k in ESP_KW): score += 5
    if any(k in p.device.lower() for k in ESP_KW): score += 3
    return score

def best_port(ports_info):
    if not ports_info: return ""
    ranked = sorted(ports_info, key=rank_port, reverse=True)
    return ranked[0].device

def refresh_ports_cb():
    ports_info = serial.tools.list_ports.comports()
    ports = [p.device for p in ports_info]
    dpg.configure_item("port_combo", items=ports)
    if ports:
        dpg.set_value("port_combo", best_port(ports_info))

def toggle_log_cb():
    global log_file, log_enabled
    if not log_enabled:
        log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs")
        os.makedirs(log_dir, exist_ok=True)
        fname = os.path.join(log_dir, f"aprs_{time.strftime('%Y%m%d_%H%M%S')}.log")
        log_file = open(fname, "w", buffering=1)
        log_enabled = True
        dpg.configure_item("btn_log", label="■ Log")
        dpg.configure_item("btn_log", enabled=True)
    else:
        log_enabled = False
        if log_file:
            log_file.close()
            log_file = None
        dpg.configure_item("btn_log", label="▶ Log")


def sv(tag, val):
    dpg.set_value(tag, str(val))

def update_cards():
    s  = state["sys"]
    sg = state["gps"]
    sb = state["bat"]
    sa = state["aprs"]
    sc = state["cfg"]
    ss = state["sb"]
    sr = state["rf"]

    # SYSTEM
    sv("v_time",   s["t"])
    sv("v_uptime", uptime_str(s["uptime"]))
    sv("v_heap",   f"{s['heap']//1024} KB")
    sv("v_cpu",    f"{s['cpu']:.1f} C")
    sv("v_rssi",   f"{s['rssi']} dBm")
    sv("v_ip",     s["ip"])

    # CONFIG
    mode_parts = []
    if sc["tnc"]:     mode_parts.append("TNC")
    if sc["digi"]:    mode_parts.append("Digi")
    if sc["aprs_is"]: mode_parts.append("APRS-IS")
    gps_mode_str = {0: "Auto", 1: "GPS", 2: "Fixed"}.get(sc["gps_mode"], str(sc["gps_mode"]))
    sv("v_call",     f"{sc['call']}-{sc['ssid']}")
    sv("v_mode",     " / ".join(mode_parts) if mode_parts else "---")
    sv("v_wifi",     wifi_str(sc["wifi"]))
    sv("v_send_mode",gps_mode_str)
    sv("v_beacon",   "SmartBeacon" if sc["sb"] else (f"{sc['bcn']} min" if sc["bcn"] else "OFF"))
    sv("v_aprs_is",  "ON" if sc["aprs_is"] else "OFF")
    routing = []
    if sc["rf2inet"]: routing.append("RF→IS")
    if sc["inet2rf"]: routing.append("IS→RF")
    sv("v_routing",  " / ".join(routing) if routing else "OFF")
    sv("v_path",     sc["path"])
    sv("v_symbol",   sc["symbol"])
    frx = sc["freq_rx"]; ftx = sc["freq_tx"]
    sv("v_freq_rx", f"{frx:.4f}" if frx else "---")
    sv("v_freq_tx", f"{ftx:.4f}" if ftx else "---")

    # BATTERY
    pct = sb["pct"]
    if pct >= 0:
        dpg.set_value("v_bat_bar", pct / 100)
        dpg.configure_item("v_bat_bar", overlay=f"  {pct}%")
    sv("v_bat_mv",   f"{sb['mv']} mV")
    sv("v_vbus",     f"{sb['vbus']} mV")
    sv("v_sys_mv",   f"{sb['sys_mv']} mV")
    sv("v_chg",      sb["chg"])
    sv("v_bat_temp", f"{sb['temp']:.1f} C")

    # GPS
    fix    = sg["fix"]
    online = sg["online"]
    fix_label = "FIX OK" if fix else ("Waiting for FIX" if online else "NO GNSS")
    fix_col   = COL_OK   if fix else (COL_WARN           if online else COL_ERR)
    dpg.configure_item("v_fix", default_value=fix_label, color=fix_col)
    if fix:
        lat = sg["lat"]; lon = sg["lon"]
        sv("v_latlon", f"{abs(lat):.6f}{'N' if lat>=0 else 'S'}    {abs(lon):.6f}{'E' if lon>=0 else 'W'}")
    else:
        sv("v_latlon", "---")
    sv("v_alt",    f"{sg['alt']:.1f} m"    if fix else "---")
    sv("v_spd",    f"{sg['spd']:.1f} km/h" if fix else "---")
    sv("v_hdg",    f"{sg['hdg']:.1f} deg"  if fix else "---")
    sv("v_sats",   f"{sg['sats']}    HDOP {sg['hdop']:.1f}")
    sv("v_const",  f"GPS:{sg['gps_n']}  BDS:{sg['bds_n']}  GLO:{sg['glo_n']}")
    sv("v_age",    f"{sg['age']} s" if fix else "---")

    # APRS 2×2
    sv("v_all",   sa["all"])
    sv("v_drop",  sa["drop"])
    sv("v_rf2in", sa["rf2in"])
    sv("v_in2rf", sa["in2rf"])

    # SB
    remaining = max(0, ss["rate_s"] - ss["elapsed_s"])
    sv("v_sb_dist", f"{ss['dist_m']} m")
    sv("v_sb_spd",  f"{ss['spd_kmh']} km/h")
    sv("v_sb_next", f"{remaining} s  (rate {ss['rate_s']} s)")
    sv("v_sb_turn", f"{ss['turn_deg']}°  /  thr {ss['thr_deg']}°  [{ss['result']}]")

    # RF
    ptt_label = "  TX  " if sr["ptt"] else " IDLE "
    ptt_col   = COL_TX   if sr["ptt"] else COL_OK
    dpg.configure_item("v_ptt", default_value=ptt_label, color=ptt_col)
    tx_pfx  = f"[{sr['tx_t']}]  "  if sr["tx_t"]  else ""
    rx_pfx  = f"[{sr['rx_t']}]  "  if sr["rx_t"]  else ""
    sms_pfx = f"[{sr['sms_t']}]  " if sr["sms_t"] else ""
    dpg.configure_item("v_last_tx",  default_value=tx_pfx  + sr["last_tx"])
    dpg.configure_item("v_last_rx",  default_value=rx_pfx  + sr["last_rx"])
    dpg.configure_item("v_last_sms", default_value=sms_pfx + sr["last_sms"])


def add_log(tag, line):
    global log_file, log_enabled
    color = TAG_COLORS.get(tag, COL_PLAIN)
    dpg.add_text(line, color=color, parent="log_win")
    children = dpg.get_item_children("log_win", slot=1)
    if len(children) > MAX_LOG:
        dpg.delete_item(children[0])
    dpg.set_y_scroll("log_win", dpg.get_y_scroll_max("log_win"))
    if log_enabled and log_file:
        try:
            log_file.write(line + "\n")
        except Exception:
            pass


def drain_queue():
    processed = 0
    while not event_queue.empty() and processed < 50:
        tag, data, line = event_queue.get_nowait()
        processed += 1

        if tag == "__connected__":
            state["connected"] = True
            dpg.configure_item("status_dot",  default_value="LIVE",          color=COL_OK)
            dpg.configure_item("btn_connect", label="  Disconnect  ",
                               callback=connect_toggle_cb, enabled=True)
            continue
        if tag == "__disconnected__":
            state["connected"] = False
            dpg.configure_item("status_dot",  default_value="OFFLINE",        color=COL_ERR)
            dpg.configure_item("btn_connect", label="  Connect  ",
                               callback=connect_toggle_cb, enabled=True)
            continue
        if tag == "__error__":
            dpg.configure_item("status_dot",  default_value=f"ERR: {line[:40]}", color=COL_ERR)
            dpg.configure_item("btn_connect", label="  Connect  ",
                               callback=connect_toggle_cb, enabled=True)
            continue

        if line:
            add_log(tag, line)

        if not data:
            continue

        if tag == "[STATUS]":
            apply_status(data)
            dpg.set_value("v_last", data.get("t", ""))
            update_cards()
        elif tag == "[DEBUG]":
            apply_debug(data)
            update_cards()
        elif tag == "[SB]":
            apply_sb(data)
            update_cards()
        elif tag == "[TX]":
            apply_tx(data)
            update_cards()
        elif tag == "[PKT]":
            apply_pkt(data)
            update_cards()
        elif tag == "[MSG]":
            apply_msg(data)
            update_cards()


def kv_table(rows, col0=90):
    with dpg.table(header_row=False, borders_innerV=False,
                   policy=dpg.mvTable_SizingFixedFit, pad_outerX=False):
        dpg.add_table_column(width_fixed=True, init_width_or_weight=col0)
        dpg.add_table_column(width_stretch=True)
        for row in rows:
            label, vtag = row[0], row[1]
            color = row[2] if len(row) > 2 else None
            with dpg.table_row():
                dpg.add_text(label, color=COL_LABEL)
                if color:
                    dpg.add_text("---", tag=vtag, color=color)
                else:
                    dpg.add_text("---", tag=vtag)

def card_header(title):
    dpg.add_text(title, color=COL_HEAD)
    dpg.add_separator()
    dpg.add_spacer(height=1)


def build():
    WIN_PAD = 8
    AVAIL   = W - WIN_PAD * 2 - 16
    GAP     = 6
    W1 = 210; W2 = 500; W3 = AVAIL - W1 - W2 - GAP * 2
    W4 = 510; W5 = AVAIL - W4 - GAP
    R1H  = 152
    R2H  = 215
    RF_H = 100

    with dpg.window(tag="main_win", no_title_bar=True, no_resize=True, no_move=True,
                    no_scrollbar=True, no_scroll_with_mouse=True,
                    width=W, height=H, pos=(0, 0)):

        # ── toolbar ──────────────────────────────────────────────
        with dpg.group(horizontal=True):
            dpg.add_text("Port:", color=COL_LABEL)
            _pi = serial.tools.list_ports.comports()
            ports = [p.device for p in _pi]
            dpg.add_combo(ports, default_value=best_port(_pi),
                          width=200, tag="port_combo")
            dpg.add_spacer(width=2)
            dpg.add_button(label="↺", callback=refresh_ports_cb, width=24)
            dpg.add_spacer(width=4)
            dpg.add_text("Baud:", color=COL_LABEL)
            dpg.add_combo(["115200", "921600"], default_value="115200",
                          width=85, tag="baud_combo")
            dpg.add_spacer(width=6)
            dpg.add_button(label="  Connect  ", tag="btn_connect", callback=connect_toggle_cb)
            dpg.add_spacer(width=8)
            dpg.add_button(label="▶ Log", tag="btn_log", callback=toggle_log_cb)
            dpg.add_spacer(width=10)
            dpg.add_text("OFFLINE", tag="status_dot", color=COL_ERR)
            dpg.add_spacer(width=12)
            dpg.add_text("Updated:", color=COL_LABEL)
            dpg.add_text("---", tag="v_last", color=(150, 150, 150, 255))
        dpg.add_text(f"v{VERSION}", pos=[W - 52, 6], color=(80, 80, 80, 255))

        dpg.add_spacer(height=5)

        # ── row 1: SYSTEM | CONFIG | BATTERY ─────────────────────
        with dpg.group(horizontal=True):
            with dpg.child_window(width=W1, height=R1H, border=True,
                                  no_scrollbar=True, no_scroll_with_mouse=True):
                card_header("SYSTEM")
                kv_table([
                    ("Time",   "v_time"),
                    ("Uptime", "v_uptime"),
                    ("Heap",   "v_heap"),
                    ("CPU",    "v_cpu"),
                    ("RSSI",   "v_rssi"),
                    ("IP",     "v_ip"),
                ], col0=52)

            dpg.add_spacer(width=GAP)

            with dpg.child_window(width=W2, height=R1H, border=True,
                                  no_scrollbar=True, no_scroll_with_mouse=True):
                card_header("CONFIG")
                with dpg.table(header_row=False, borders_innerV=False,
                               policy=dpg.mvTable_SizingFixedFit, pad_outerX=False):
                    dpg.add_table_column(width_fixed=True,  init_width_or_weight=76)
                    dpg.add_table_column(width_stretch=True)
                    dpg.add_table_column(width_fixed=True,  init_width_or_weight=76)
                    dpg.add_table_column(width_stretch=True)
                    rows4 = [
                        ("Callsign",  "v_call",      "Mode",      "v_mode"),
                        ("Path",      "v_path",      "WiFi",      "v_wifi"),
                        ("Beacon",    "v_beacon",    "Symbol",    "v_symbol"),
                        ("Send mode", "v_send_mode", "Routing",   "v_routing"),
                        ("RX",        "v_freq_rx",   "APRS-IS",   "v_aprs_is"),
                        ("TX",        "v_freq_tx",   "",          None),
                    ]
                    for l1, t1, l2, t2 in rows4:
                        with dpg.table_row():
                            dpg.add_text(l1, color=COL_LABEL)
                            dpg.add_text("---", tag=t1)
                            dpg.add_text(l2, color=COL_LABEL)
                            if t2:
                                dpg.add_text("---", tag=t2)
                            else:
                                dpg.add_text("")

            dpg.add_spacer(width=GAP)

            with dpg.child_window(width=W3, height=R1H, border=True,
                                  no_scrollbar=True, no_scroll_with_mouse=True):
                card_header("BATTERY")
                dpg.add_progress_bar(default_value=0, width=-1, height=14,
                                     overlay="  ---%", tag="v_bat_bar")
                dpg.add_spacer(height=1)
                kv_table([
                    ("Voltage", "v_bat_mv"),
                    ("Vbus",    "v_vbus"),
                    ("System",  "v_sys_mv"),
                    ("Charge",  "v_chg"),
                    ("Temp",    "v_bat_temp"),
                ], col0=68)

        dpg.add_spacer(height=GAP)

        # ── row 2: GPS | APRS + SB ───────────────────────────────
        with dpg.group(horizontal=True):
            with dpg.child_window(width=W4, height=R2H, border=True,
                                  no_scrollbar=True, no_scroll_with_mouse=True):
                card_header("GPS")
                with dpg.group(horizontal=True):
                    dpg.add_text("Status", color=COL_LABEL)
                    dpg.add_spacer(width=10)
                    dpg.add_text("NO GNSS", tag="v_fix", color=COL_ERR)
                kv_table([
                    ("Lat / Lon",   "v_latlon"),
                    ("Altitude",    "v_alt"),
                    ("Speed",       "v_spd"),
                    ("Heading",     "v_hdg"),
                    ("Sats / HDOP", "v_sats"),
                    ("Constell.",   "v_const"),
                    ("Age",         "v_age"),
                ], col0=76)

            dpg.add_spacer(width=GAP)

            with dpg.child_window(width=W5, height=R2H, border=True,
                                  no_scrollbar=True, no_scroll_with_mouse=True):
                card_header("APRS")
                # 2×2 grid
                with dpg.table(header_row=False, borders_innerV=False,
                               policy=dpg.mvTable_SizingStretchSame, pad_outerX=False):
                    dpg.add_table_column()
                    dpg.add_table_column()
                    with dpg.table_row():
                        with dpg.group():
                            dpg.add_text("All packets", color=COL_LABEL)
                            dpg.add_text("---", tag="v_all")
                        with dpg.group():
                            dpg.add_text("Dropped", color=COL_LABEL)
                            dpg.add_text("---", tag="v_drop")
                    with dpg.table_row():
                        with dpg.group():
                            dpg.add_text("RF→INET", color=COL_LABEL)
                            dpg.add_text("---", tag="v_rf2in")
                        with dpg.group():
                            dpg.add_text("INET→RF", color=COL_LABEL)
                            dpg.add_text("---", tag="v_in2rf")

                dpg.add_separator()
                dpg.add_text("SmartBeacon", color=COL_HEAD)
                kv_table([
                    ("Distance",   "v_sb_dist"),
                    ("Speed",      "v_sb_spd"),
                    ("Next TX",    "v_sb_next"),
                    ("Turn / Thr", "v_sb_turn"),
                ], col0=72)

        dpg.add_spacer(height=GAP)

        # ── row 3: RF status (full width) ────────────────────────
        with dpg.child_window(width=AVAIL, height=RF_H, border=True,
                              no_scrollbar=True, no_scroll_with_mouse=True):
            card_header("RF")
            # PTT label + Last TX value on one row
            with dpg.table(header_row=False, borders_innerV=False,
                           policy=dpg.mvTable_SizingFixedFit, pad_outerX=False):
                dpg.add_table_column(width_fixed=True, init_width_or_weight=48)   # "PTT"
                dpg.add_table_column(width_fixed=True, init_width_or_weight=60)   # IDLE/TX
                dpg.add_table_column(width_fixed=True, init_width_or_weight=70)   # "Last TX"
                dpg.add_table_column(width_stretch=True)
                with dpg.table_row():
                    dpg.add_text("PTT",     color=COL_LABEL)
                    dpg.add_text(" IDLE ",  tag="v_ptt", color=COL_OK)
                    dpg.add_text("Last TX", color=COL_LABEL)
                    dpg.add_text("---",     tag="v_last_tx", wrap=0)
                with dpg.table_row():
                    dpg.add_text("")
                    dpg.add_text("")
                    dpg.add_text("Last RX",  color=COL_LABEL)
                    dpg.add_text("---",      tag="v_last_rx",
                                 color=(180, 220, 255, 255), wrap=0)
                with dpg.table_row():
                    dpg.add_text("")
                    dpg.add_text("")
                    dpg.add_text("Last SMS", color=(255, 200, 80, 255))
                    dpg.add_text("---",      tag="v_last_sms",
                                 color=(255, 220, 120, 255), wrap=0)

        dpg.add_spacer(height=GAP)

        # ── LOG ──────────────────────────────────────────────────
        dpg.add_child_window(width=AVAIL, height=-1, border=True,
                             horizontal_scrollbar=True, tag="log_win")
        dpg.add_text("LOG", color=COL_HEAD, parent="log_win")
        dpg.add_separator(parent="log_win")


# ── init ─────────────────────────────────────────────────────────────────────
dpg.create_context()

with dpg.theme() as global_theme:
    with dpg.theme_component(dpg.mvAll):
        dpg.add_theme_style(dpg.mvStyleVar_ItemSpacing,   4, 2)
        dpg.add_theme_style(dpg.mvStyleVar_CellPadding,   4, 1)
        dpg.add_theme_style(dpg.mvStyleVar_FramePadding,  4, 2)
        dpg.add_theme_style(dpg.mvStyleVar_WindowPadding, 8, 4)
dpg.bind_theme(global_theme)

with dpg.font_registry():
    with dpg.font("/System/Library/Fonts/Menlo.ttc", 13) as default_font:
        pass
dpg.bind_font(default_font)

dpg.create_viewport(title=f"APRS-ESP Monitor v{VERSION}", width=W, height=H, resizable=True)
dpg.setup_dearpygui()
build()
dpg.set_primary_window("main_win", True)
dpg.show_viewport()

_last_port_scan = 0.0
_last_ports: list = []

while dpg.is_dearpygui_running():
    drain_queue()
    if not state["connected"]:
        now = time.time()
        if now - _last_port_scan >= 2.0:
            _last_port_scan = now
            ports = [p.device for p in serial.tools.list_ports.comports()]
            if ports != _last_ports:
                _last_ports = ports
                dpg.configure_item("port_combo", items=ports)
                if ports and dpg.get_value("port_combo") not in ports:
                    dpg.set_value("port_combo", ports[0])
    dpg.render_dearpygui_frame()

stop_event.set()
if log_file:
    log_file.close()
dpg.destroy_context()
