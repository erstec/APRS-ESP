/*
    Rotary encoder + button library for ESP32.
    Encoder uses Ben Buxton's full-step state table (GPL2).
    Button adds long-press detection (LONG_PRESS_MS threshold).

    BTN_RELEASED and BTN_RELEASED_LONG are mutually exclusive:
    - short press  → BTN_PRESSED … BTN_RELEASED
    - long press   → BTN_PRESSED … BTN_PRESSED_LONG … BTN_RELEASED_LONG

    Interrupt-driven usage:
      1. Call begin() to configure pin modes.
      2. Attach GPIO interrupts (CHANGE) for CLK and DT pins to a IRAM_ATTR ISR
         that calls rotary.processISR().
      3. In the main task, call consumePending() to get the accumulated direction
         (DIR_CW, DIR_CCW, or DIR_NONE) and process_button() for button state.
*/
#pragma once
#include <Arduino.h>

#define DIR_NONE  0x00
#define DIR_CW    0x10
#define DIR_CCW   0x20

#define BTN_NONE          0
#define BTN_PRESSED       1
#define BTN_RELEASED      2
#define BTN_PRESSED_LONG  3
#define BTN_RELEASED_LONG 4

#define LONG_PRESS_MS 800UL

class Rotary {
public:
    Rotary(uint8_t clk, uint8_t dt, uint8_t btn);
    void begin();
    void IRAM_ATTR processISR();    // call from GPIO ISR on CLK or DT CHANGE
    uint8_t consumePending();       // call from main task; returns DIR_CW/CCW/NONE
    unsigned char process_button();

private:
    uint8_t  _clk, _dt, _btn;
    uint8_t  _encState;
    volatile uint8_t _pendingDir;
    bool     _btnDown;
    bool     _longFired;
    unsigned long _pressedAt;
};
