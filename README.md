# ATTENTION: WORK IN PROGRESS, BOTH FW AND HW / ASK [ME](https://github.com/erstec) BY EMAIL OR DISCORD BEFORE USING IT !!!
# This notice will be removed as soon as thing will be tested and stable to reproduce

# NOTICE
This fork of project combines few projects, plus my own HW and FW implementation

Firmware part uses ideas and code snippets from:
- https://github.com/nakhonthai/ESP32IGate
- https://github.com/sh123/aprs_tracker

Hardware part uses ideas, libraries and designs from:
- https://github.com/nakhonthai/ESP32IGate / schematics base
- https://github.com/handiko/Dorji-TX-Shield / board base
- some another Eagle libraries from various sources

## Summary:
FW:
- IGate
- Digipeater
- Position beaconing
- SmartBeaconing
- OLED display operation states and etc.
- All parameters changeable via WEB interface
- Some parameters changeable via OLED / Rotary encoder
HW:
- ESP32-DevKitC-v4
- SA818 RF Modem
- Any NMEA output serial GNSS receiver
- SSD106 0.96" OLED screen
- Rotary encoder
- Regulated buck converted (LM2596S based)
- PCB ready (Gerber and Eagle) / BOM in /hardware folder

# APRS-ESP32 Project

APRS-ESP32 is a Tracker + Digipeater + Internet Gateway + TNC Built in that is implemented for Espressif ESP32 MCU.

## Feature

* Development based on PlatformIO / Visual Studio Code IDE
* Support Bell202 1200bps AFSK
* Implementing software modem, decoding and encoding
* ---about >800 packets can be decoded against WA8LMF TNC TEST CD Track 1 (MP3)
* ---support TNC2 Raw protocol only
* Using USB serial for host connection and power supply
* Support Wi-Fi connection (TCP and UDP) to APRS-IS
* Support Web Service config and control system
* Display received and transmit packet on the LED

APRS-ESP32 is small interface board with SA8x8 RF Module ---for connecting to a transceiver.

* PCB size is XxY mm
* DC barrel socker for powering board (5.5x2.1mm with pin positive, 7-40V DC)
* 

### Schematic

---

### CAD data
 
The gerber data is [here](doc/Gerber_.zip)

The PCB Layout is [here](doc/PCB_Layout.pdf)

The Schematic PDF is [here](doc/Schematic.pdf)

### BOM list  

|Reference|Value|Description|
|---|:---:|---|
|U1|ESP32 DEVKIT|DOIT ESP32 DEVKIT (โมดูล ESP32)|

### Mounting drawing

![mounting](image/_SimpleLayout.png)

## Howto Devellop
- Pull and Compile by PlatformIO on the Visual Studio Code.

## APRS-ESP32 firmware installation (do it first, next, update via web)
- 1. Connect the USB cable to the ESP32 Module.
- 2. Download firmware and open the program ESP32 DOWNLOAD TOOL, set it in the firmware upload program, set the firmware to APRS-ESP32_Vxx.bin, location 0x10000 and partitions.bin at 0x8000 and bootloader.bin at 0x1000 and boot.bin at 0xe000, if not loaded, connect GPIO0 cable to GND, press START button finished, press power button or reset (red) again.
- 3. Then go to WiFi AP SSID: APRS-ESP32 and open a browser to the website. http://192.168.4.1 password: aprs
- 4. Push BOOT button: >100ms - TX Position, >10Sec - Reset to Factory Default

![ESP32Tool](image/ESP32Tool.png)

## ESP32 Flash Download Tools
https://www.espressif.com/en/support/download/other-tools

## HITH
---This project implement by APRS text (TNC2 Raw) only,It not support null string(0x00) in the package.
