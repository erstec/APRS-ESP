// SA8x8 radio module
#define VBAT_PIN            35
#define POWER_PIN           12
#define PULLDOWN_PIN        27
#define SQL_PIN             33

// Interface to the radio / radio module
#define SPK_PIN             ADC1_CHANNEL_0  // Read ADC1_0 From PIN 36(VP)
#define MIC_PIN             26              // Out wave to PIN 26
#define PTT_PIN             25
#define RX_LED_PIN          2
#define TX_LED_PIN          4
#define RSSI_PIN            33

// RF UART
#define SERIAL_RF_UART      1
#define SERIAL_RF_BAUD      9600
#define SERIAL_RF_RXPIN     14
#define SERIAL_RF_TXPIN     13

// GPS UART
#define SERIAL_GPS_UART     2
#define SERIAL_GPS_BAUD     4800
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
