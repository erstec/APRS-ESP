/*
    Description:    This file is part of the APRS-ESP project.
                    This file contains the code for pinouts / settings and etc.
    Author:         Ernest (ErNis) / LY3PH
    License:        GNU General Public License v3.0
    Includes code from:
                    https://github.com/nakhonthai/ESP32IGate
*/

#define BOOT_PIN            0

#if defined(BOARD_ESP32DR)

#define BOARD_NAME          "ESP32DR"

// SA8x8 radio module
//#define VBAT_PIN            35
//#define POWER_PIN           12
//#define POWER_PIN           23
#define POWER_PIN           25
#define POWERDOWN_PIN       27
#define SQL_PIN             33

// Interface to the radio / radio module
#define SPK_PIN             ADC1_CHANNEL_0  // Read ADC1_0 From PIN 36(VP)
#define MIC_PIN             26              // Out wave to PIN 26
//#define PTT_PIN             25
#define PTT_PIN             32
#define RX_LED_PIN          2
#define TX_LED_PIN          4
#define RSSI_PIN            33

// DEBUG UART
#define SERIAL_DEBUG_BAUD   115200

// RF UART
#define SERIAL_RF_UART      1
#define SERIAL_RF_BAUD      9600
#define SERIAL_RF_RXPIN     14
#define SERIAL_RF_TXPIN     13

// GPS UART
#define SERIAL_GPS_UART     2
#define SERIAL_GPS_BAUD     9600
#define SERIAL_GPS_RXPIN    16
#define SERIAL_GPS_TXPIN    17

// TNC UART
#define SERIAL_TNC_UART     2
#define SERIAL_TNC_BAUD     9600
#define SERIAL_TNC_RXPIN    16
#define SERIAL_TNC_TXPIN    17

// I2C OLED
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_SDA_PIN        21
#define OLED_SCL_PIN        22
#define OLED_RST_PIN        -1

// ROTARY ENCODER
#define PIN_ROT_CLK         18
#define PIN_ROT_DT          19
#define PIN_ROT_BTN         5   // IO0 may be handled too?

#endif /* BOARD_ESP32DR */

#if defined(BOARD_TTWR)

#define BOARD_NAME          "T-TWR"

// SA8x8 radio module
//#define VBAT_PIN            35
#define POWER_PIN           39
#define POWERDOWN_PIN       40
#define SQL_PIN             42

// Interface to the radio / radio module
#define SPK_PIN             ADC1_GPIO10_CHANNEL
#define MIC_PIN             18
#define PTT_PIN             41
#define RX_LED_PIN          2
#define TX_LED_PIN          1
#define RSSI_PIN            42

// DEBUG UART
#define SERIAL_DEBUG_BAUD   115200

// RF UART
#define SERIAL_RF_UART      1
#define SERIAL_RF_BAUD      9600
#define SERIAL_RF_RXPIN     48
#define SERIAL_RF_TXPIN     47

// GPS UART
#define SERIAL_GPS_UART     2
#define SERIAL_GPS_BAUD     9600
#define SERIAL_GPS_RXPIN    17
#define SERIAL_GPS_TXPIN    16

// TNC UART
#define SERIAL_TNC_UART     2
#define SERIAL_TNC_BAUD     9600
#define SERIAL_TNC_RXPIN    17
#define SERIAL_TNC_TXPIN    16

// I2C OLED
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_SDA_PIN        13
#define OLED_SCL_PIN        14
#define OLED_RST_PIN        21 // POWER ENABLE

// ROTARY ENCODER
#define PIN_ROT_CLK         9
#define PIN_ROT_DT          5
#define PIN_ROT_BTN         7

#endif /* BOARD_TTWR */
