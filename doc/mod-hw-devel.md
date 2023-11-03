# ONLY FOR DEVELOPMENT / INCOMPLETE

## 3D Print Cases for T-TWL PLUS
https://github.com/OpenRTX/ttwrplus-case

https://github.com/k5jae/LILYGO-T-TWR-Plus-case

## USING LILYGO T-TWR V1.4 BOARD - MODIFICATIONS REQUIRED
### Introduction
LilyGo T-TWR V1.4 is a compact board with ESP32-S3 MCU, SA868 2W RF Module, OLED display, USB-C connector, 18650 battery holder, and integrated LiPo charger.
This board is suitable for APRS-ESP32 project. This board have some hardware limitations, that need to be addressed, like abscence of audio routing between SA868 RF modem and ESP32 MCU, as well as audio level and filtering circuits.
I've bought few of these boards and made some modifications to make it work with APRS-ESP32 project.

### READ THIS FIRST
This will require *good* soldering skills and some hardware modifications.
You will need to have a soldering iron with a fine tip, and a lot of patience.
Don't try this if you are not sure what you are doing.
You WILL lose your warranty if you do this, don't claim anything from the seller or me if you break your board. YOU DOING THIS ON YOUR OWN RISK! YOU ARE WARNED!

For me it took about 2 hours to make all modifications, but I have all required tools and experience.

### Required Components
- 1x Audio Adapter Board (link here), order PCB and assemble yourself, BOM (here (link here))
- Few wires, I'm using silicone coated, 1mm diameter, 0.5mm core
- Soldering wire, flux, iron, hot air station, tweezers, isopropyl alcohol, soldering wick, double sided tape, etc.

### General Soldering
- Power off the board, remove battery, unplug USB cable
- Remove Microphone (Pic. 1, marked)
- Remove Mic pull-up Resistor R2 (Pic. 1, marked)
- Detach and remove (unsolder) Speaker (Pic. 2, marked)
- Remove Speaker to AMP resistor RF1 (Pic. 2, marked)
- Remove Green LED resistor (marked)
- If you want to use HIGH RF power remove battery protection cirquit (Pic. 3, marked) and short two marked pads with piece of wire.

### Wiring / Audio Adapter Board
- Prepare six wires, approx 10 cm long, strip and tin one side of each wire.
- Solder one side of each wire to the pads of the Audio Adapter Board, as shown on the picture.
- Optional: you can cover Audio Adapter PCB with heatshrink tube to protect it from touching with fingers, as it will temporary breaks audio routing.
- Take prepared and assembled Audio Adapter Board, apply double sided tape to the bottom side of the board, align it and stick to the top of ESP32 MCU shield.
- Measure and cut each wire to required length. Solder other side of each wire to the pads, as shown on the picture. Black - to GND. Red - to 3V3. Yellow to 18. Blue to ESP32 pad IO1. White - to Mic + pad and Orange to SA868 module pad 3 (marked).

### Programming
- Connect USB cable. DON'T INSTALL BATTERY AT THIS MOMENT!
- Upload firmware to the board using your favorite tool or instructions proviided on main readme page. I'm using PlatformIO IDE.
- After firmware is uploaded, shortly press Reset button. Board should start and OLED should show status screen.
- Configure using WiFi AP mode. Don't forget to set your callsign and SSID, frequency, etc.

## Connecting GPS Module
- You can use any GPS module with UART interface. 3.3V powered are preferable. I'm using NEO-6M module. Baudrate is 9600bps.
- Connect GPS module to the board using 3V3, GND, TX (IO18) and RX (IO17) pads.
