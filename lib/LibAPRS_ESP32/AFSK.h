#ifndef AFSK_H
#define AFSK_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pgmspace.h>
#include "FIFO.h"
#include "HDLC.h"

#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
// SIN_LEN=240 with DAC 48kHz: MARK_INC=6→1200.0Hz, SPACE_INC=11→2200.0Hz (exact)
#define SIN_LEN 240
static const uint8_t sin_table[] = {
        128, 131, 135, 138, 141, 145, 148, 151, 154, 158, 161, 164, 167, 170, 174, 177,
        180, 183, 186, 189, 192, 194, 197, 200, 203, 205, 208, 210, 213, 215, 218, 220,
        222, 225, 227, 229, 231, 233, 235, 236, 238, 240, 241, 243, 244, 245, 247, 248,
        249, 250, 251, 251, 252, 253, 253, 254, 254, 255, 255, 255,
};
#else
// SIN_LEN=192 with DAC 38.4kHz: MARK_INC=6→1200.0Hz, SPACE_INC=11→2200.0Hz (exact)
#define SIN_LEN 192
static const uint8_t sin_table[] = {
        128, 132, 136, 140, 145, 149, 153, 157, 161, 165, 169, 173, 177, 180, 184, 188,
        192, 195, 199, 202, 205, 209, 212, 215, 218, 221, 223, 226, 229, 231, 234, 236,
        238, 240, 242, 244, 245, 247, 248, 250, 251, 252, 253, 253, 254, 254, 255, 255,
};
#endif

inline static uint8_t sinSample(uint16_t i)
{
    uint16_t newI = i % (SIN_LEN / 2);
    newI = (newI >= (SIN_LEN / 4)) ? (SIN_LEN / 2 - newI - 1) : newI;
    uint8_t sine = pgm_read_byte(&sin_table[newI]);
    return (i >= (SIN_LEN / 2)) ? (255 - sine) : sine;
}

#define SWITCH_TONE(inc) (((inc) == MARK_INC) ? SPACE_INC : MARK_INC)
#define BITS_DIFFER(bits1, bits2) (((bits1) ^ (bits2)) & 0x01)
#define DUAL_XOR(bits1, bits2) ((((bits1) ^ (bits2)) & 0x03) == 0x03)
#define SIGNAL_TRANSITIONED(bits) DUAL_XOR((bits), (bits) >> 2)
#define TRANSITION_FOUND(bits) BITS_DIFFER((bits), (bits) >> 1)

#define CPU_FREQ F_CPU

#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
#define CONFIG_AFSK_RX_BUFLEN 350
#define CONFIG_AFSK_TX_BUFLEN 350
#else
#define CONFIG_AFSK_RX_BUFLEN 50
#define CONFIG_AFSK_TX_BUFLEN 50
#endif
#define CONFIG_AFSK_RXTIMEOUT 0
#define CONFIG_AFSK_PREAMBLE_LEN 350UL
#define CONFIG_AFSK_TRAILER_LEN 50UL
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
#define CONFIG_AFSK_DAC_SAMPLERATE 48000
#define SAMPLERATE 9600
#else
#define CONFIG_AFSK_DAC_SAMPLERATE 38400
#define SAMPLERATE 38400
#endif
#define BITRATE 1200
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
#define SAMPLESPERBIT (CONFIG_AFSK_DAC_SAMPLERATE / BITRATE)
#else
#define SAMPLESPERBIT (SAMPLERATE / BITRATE)
#endif
#define BIT_STUFF_LEN 5
#define MARK_FREQ 1200
#define SPACE_FREQ 2200
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
#define PHASE_BITS 8                                    // 8 How much to increment phase counter each sample
#define PHASE_INC 1                                     // 1 Nudge by an eigth of a sample each adjustment
#define PHASE_MAX ((SAMPLERATE / BITRATE) * PHASE_BITS) // Resolution of our phase counter = 64
#define PHASE_THRESHOLD (PHASE_MAX / 2)                 // Target transition point of our phase window
#else
#define PHASE_BITS 32                           // How much to increment phase counter each sample
#define PHASE_INC 4                            // Nudge by an eigth of a sample each adjustment
#define PHASE_MAX (SAMPLESPERBIT * PHASE_BITS) // Resolution of our phase counter = 64
#define PHASE_THRESHOLD (PHASE_MAX / 2)        // Target transition point of our phase window

#define I2S_INTERNAL
#endif

// #define SQL

#include "../../include/pinout.h"

#ifdef I2S_INTERNAL
#include "driver/i2s.h"
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "driver/dac.h"
#endif

#define SAMPLE_RATE SAMPLERATE //9580 ปรับชดเชยแซมปลิงค์ของ I2S
#define PIN_I2S_BCLK    26
#define PIN_I2S_LRC     27
#define PIN_I2S_DIN     35
#define PIN_I2S_DOUT    25

typedef enum
{
    LEFTCHANNEL = 0,
    RIGHTCHANNEL = 1
} SampleIndex;
/// @parameter MODE : I2S_MODE_RX or I2S_MODE_TX
/// @parameter BPS : I2S_BITS_PER_SAMPLE_16BIT or I2S_BITS_PER_SAMPLE_32BIT
void I2S_Init(i2s_mode_t MODE, i2s_bits_per_sample_t BPS);
#endif

typedef struct Hdlc
{
    uint8_t demodulatedBits;
    uint8_t bitIndex;
    uint8_t currentByte;
    bool receiving;
} Hdlc;

typedef struct Afsk
{
    // Stream access to modem
    FILE fd;

    // General values
    Hdlc hdlc;               // We need a link control structure
    uint16_t preambleLength; // Length of sync preamble
    uint16_t tailLength;     // Length of transmission tail

    // Modulation values
    uint8_t sampleIndex;       // Current sample index for outgoing bit
    uint8_t currentOutputByte; // Current byte to be modulated
    uint8_t txBit;             // Mask of current modulated bit
    bool bitStuff;             // Whether bitstuffing is allowed

    uint8_t bitstuffCount; // Counter for bit-stuffing

    uint16_t phaseAcc; // Phase accumulator
    uint16_t phaseInc; // Phase increment per sample

    FIFOBuffer txFifo;                     // FIFO for transmit data
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
    uint16_t txBuf[CONFIG_AFSK_TX_BUFLEN]; // Actial data storage for said FIFO
#else
    unsigned char txBuf[CONFIG_AFSK_TX_BUFLEN]; // Actial data storage for said FIFO
#endif

    volatile bool sending; // Set when modem is sending

    FIFOBuffer rxFifo;                     // FIFO for received data
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
    uint16_t rxBuf[CONFIG_AFSK_RX_BUFLEN]; // Actual data storage for said FIFO
#else
    unsigned char rxBuf[CONFIG_AFSK_RX_BUFLEN]; // Actual data storage for said FIFO
#endif

    int iirX[2]; // IIR Filter X cells
    int iirY[2]; // IIR Filter Y cells

    uint16_t sampledBits; // Bits sampled by the demodulator (at ADC speed)
#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
    int8_t currentPhase;  // Current phase of the demodulator
#else
    int16_t currentPhase; // Current phase of the demodulator
#endif
    uint8_t actualBits;   // Actual found bits at correct bitrate

    volatile int status; // Status of the modem, 0 means OK

} Afsk;

#define DIV_ROUND(dividend, divisor) (((dividend) + (divisor) / 2) / (divisor))
#define MARK_INC (uint16_t)(DIV_ROUND(SIN_LEN * (uint32_t)MARK_FREQ, CONFIG_AFSK_DAC_SAMPLERATE))
#define SPACE_INC (uint16_t)(DIV_ROUND(SIN_LEN * (uint32_t)SPACE_FREQ, CONFIG_AFSK_DAC_SAMPLERATE))

#include <Adafruit_NeoPixel.h>
extern Adafruit_NeoPixel strip;

#define AFSK_DAC_IRQ_START()         \
    do                               \
    {                                \
        extern bool hw_afsk_dac_isr; \
        hw_afsk_dac_isr = true;      \
    } while (0)
#define AFSK_DAC_IRQ_STOP()          \
    do                               \
    {                                \
        extern bool hw_afsk_dac_isr; \
        hw_afsk_dac_isr = false;     \
    } while (0)

// Here's some macros for controlling the RX/TX LEDs
// THE _INIT() functions writes to the DDRB register
// to configure the pins as output pins, and the _ON()
// and _OFF() functions writes to the PORT registers
// to turn the pins on or off.

#if defined(USE_NEOPIXEL)
#define RX_LED_ON() strip.setPixelColor(0, 0, 255, 0);  // Green
#define RX_LED_OFF() strip.setPixelColor(0, 0, 0, 0);   // Off
#else
#if defined(INVERT_LEDS)
#define RX_LED_ON() if (RX_LED_PIN > -1) digitalWrite(RX_LED_PIN, LOW);
#define RX_LED_OFF() if (RX_LED_PIN > -1) digitalWrite(RX_LED_PIN, HIGH);
#else
#define RX_LED_ON() if (RX_LED_PIN > -1) digitalWrite(RX_LED_PIN, HIGH);
#define RX_LED_OFF() if (RX_LED_PIN > -1) digitalWrite(RX_LED_PIN, LOW);
#endif
#endif

extern bool input_HPF;

extern Afsk *AFSK_modem;


void AFSK_init(Afsk *afsk);
void AFSK_setRxAtt(bool _rx_att);
void AFSK_transmit(char *buffer, size_t size);
void AFSK_poll(Afsk *afsk);

bool getReceive();

#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
bool getTransmit();
#endif

void afsk_putchar(char c);
int afsk_getchar(void);
void AFSK_Poll(bool isRF);
void AFSK_TimerEnable(bool sts);

void adcActive(bool sts);

#if defined(BOARD_TTWR_PLUS) || defined(BOARD_TTWR_V1)
uint8_t AFSK_dac_isr(Afsk *afsk);
esp_err_t adc_init();
void afskSetHPF(bool val);
void afskSetBPF(bool val);
// int IRAM_ATTR
int read_adc_dma(uint32_t *ret_num, uint8_t *result);
#endif

#endif
