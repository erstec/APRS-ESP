# FW and HW are tested and stable

Feel free to ask [Me](https://github.com/erstec) by creating [Discussion](https://github.com/erstec/APRS-ESP/discussions/new), by e-mail or via Discord.

# APRS-ESP32 Project

APRS-ESP32 is a APRS AFSK1200 Tracker + Digipeater + Internet Gateway implemented for Espressif ESP32/ESP323S3 MCUs.

APRS-ESP32 is a small interface board with SA8x8 RF Module on it designed to work standalone, without any USB connection.

**Also this project can work on LilyGo T-TWR Plus hardware without any modifications or on LilyGo T-TWR V1 with some HW mods.**

## Features
### FW
- IGate
- Digipeater
- Tracker
- Fixed Position beaconing (periodical)
- Item beaconing (periodical)
- SmartBeaconing functionality
- OLED display operation states
- Confiruration Backup and Restore in JSON file format
- All parameters can be changed via WEB interface or by downloading configuration JSON, editing it and reuploading back to device.

### HW
*Self built*
- ESP32-DevKitC-v4 (or any pin/size compatible)
- SA818/SA868 RF Modem
- Any NMEA output serial (UART) GNSS Receiver
- SSD1306 0.96" / SH1106 1.3" OLED Screen
- Rotary encoder
- Adjustable Regulated buck converter (LM2596S based)
- PCB (Gerber and Eagle), BOMs in /hardware folder

*Ready to use*
- LilyGO T-TWR Plus (Rev. 2.0) / First batches have wrong resistor onboard, use 2.5dB attenuation with them, default is 11dB (can be selected on Radio tab)
- LilyGO T-TWR V1 (need additional hardware modifications, read [here](doc/mod-hw-devel.md))

### Details
* Development based on PlatformIO / Visual Studio Code IDE
* Support Bell202 1200bps AFSK modulation
* Software modem for decoding and encoding
* Using USB serial for host connection and power supply
* DC barrel socker for powering board (5.5x2.1mm with pin positive, 7-40V DC)
* XT60 socket for powering from 2S-6S LiPo battery
* Support Wi-Fi connection to APRS-IS
* Support Web Service config and control system
* Display status on the OLED

### Schematics

![SCH](doc/APRS-ESP32_SA8x8_Rev_D_Schematics.png)

### PCB

![PCB](doc/APRS-ESP32_SA8x8_Rev_D_PCB.png)

### CAD data
 
GERBER data is [here](hardware/GERBER/APRS-ESP32_SA8x8_V1.7_Rev_D_2022-10-27.zip)

PCB Layout is [here](hardware/APRS-ESP32_SA8x8_Rev_D_PCB.pdf)

Schematic PDF is [here](hardware/APRS-ESP32_SA8x8_Rev_D_Schematics.pdf)

### BOM list  

Bill Of Materials (BOM) is [here](hardware/APRS-ESP32_SA8x8_Rev_D_BOM.txt)

## FLASHING FIRMWARE
### First time only. Later update via Web Interface or via USB using device-update script in Release .zip archive

### Using Pecompiled Binaries
- Connect the USB cable to the ESP32 Module while BOOT button pressed (development boards don't need Boot button to be pressed)
- [Download](https://github.com/erstec/releases) required firmware archive file
- Extract it to folder
- For initial upload use device-install.bat (Windows) or device-install.sh (Linux) script. Script is self explanatory if you run it without arguments. Also it requires Python 3 and esp-tool library, you can install it using `pip install esptool`. Use your hardware compatible .bin file **without -update** suffix from Relase archive
- Restart ESP32 Module (replug USB or press Reset button), OLED should light up and show status as well as IP address of device
- Search WiFi for AP SSID APRS-ESP32, Password: aprs-esp32
- Open a browser and go to address http://192.168.4.1
- Make all required configuration

### Building yourself
- Pull and Compile with PlatformIO on the Visual Studio Code
- Adjust settings (if needed) in `main.h` and `pinout.h`
- Select correct Target
- Connect USB cable to the ESP32 module
- Build and Upload using PlatformIO buttons
- Search WiFi for AP SSID APRS-ESP32, Password: aprs-esp32
- Open a browser and go to address http://192.168.4.1
- Make all required configuration

## UPDATING FIRMWARE
- For update device already running APRS-ESP firmware use .bin file suitable for your hardware **with -update suffix**
- Firmware can be updated via **Web interface** or using **USB cable**
- For Web interface option use Firmware tab
- For USB option use script device-update.bat (Windows) or device-update.sh (Linux). Script is self explanatory if you run it without arguments. Also it requires Python 3 and esp-tool library, you can install it using `pip install esptool`
- **DO NOT use device-install scripts for update or firmware binary files without -update suffix as you can LOST ALL CONFIGURATION!**
- **After EVERY update - save Backup, as there is no guarantee older can be used**

## NOTES
### Press and hold BOOT button or Rotary Wheel:
* Shortly (>100 ms) - TX Position
* 5 sec - Turn WiFi ON/OFF
* 10 sec - Turn RF Modem ON/OFF
* 15 sec - Reset Configuration to Factory Default
* 20 sec - Cancel button action
  
_Release when see appropriate message on screen_

### LilyGo T-TWR Plus OLED data corruption issue:
* If you have configured WiFi, but it is not connected to the network, but actively trying to do that, you can see corrupted data on OLED screen, which can't be restored without full power cycle of device. This does not affect device operation, but can be annoying. To fix this issue, please disable WiFi in configuration (both AP and STA) (or long press button and release it when OLED show WiFi SW message) or make sure configured WiFi AP are in range. This issue affects only hardware with SH1106 OLED screen.

# COPYRIGHT NOTICE
Firmware part uses ideas and code snippets from:
- https://github.com/nakhonthai/ESP32IGate
- https://github.com/sh123/aprs_tracker
- https://github.com/meshtastic/firmware / build system, scripts and workflows implementation

Hardware part uses ideas, libraries and designs from:
- https://github.com/nakhonthai/ESP32IGate / schematics base
- https://github.com/handiko/Dorji-TX-Shield / board base
- some another Eagle libraries from various sources
