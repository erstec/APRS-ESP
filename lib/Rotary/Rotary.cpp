#include "Rotary.h"

// Full-step quadrature decoder state table — Ben Buxton (GPL2)
// Columns: pinstate = (digitalRead(CLK) << 1) | digitalRead(DT)
// Output bits 0x30: DIR_CW (0x10) or DIR_CCW (0x20) on completed step
// DRAM_ATTR: must be in DRAM so processISR() can access it safely from ISR context
static const DRAM_ATTR uint8_t TTABLE[7][4] = {
//   00    01    10    11
  {0x00, 0x02, 0x04, 0x00},  // 0 R_START
  {0x03, 0x00, 0x01, 0x10},  // 1 R_CW_FINAL   → DIR_CW on [11]
  {0x03, 0x02, 0x00, 0x00},  // 2 R_CW_BEGIN
  {0x03, 0x02, 0x01, 0x00},  // 3 R_CW_NEXT
  {0x06, 0x00, 0x04, 0x00},  // 4 R_CCW_BEGIN
  {0x06, 0x05, 0x00, 0x20},  // 5 R_CCW_FINAL  → DIR_CCW on [11]
  {0x06, 0x05, 0x04, 0x00},  // 6 R_CCW_NEXT
};

Rotary::Rotary(uint8_t clk, uint8_t dt, uint8_t btn)
    : _clk(clk), _dt(dt), _btn(btn),
      _encState(0), _pendingDir(DIR_NONE),
      _btnDown(false), _longFired(false), _pressedAt(0) {}

void Rotary::begin() {
    pinMode(_clk, INPUT_PULLUP);
    pinMode(_dt,  INPUT_PULLUP);
    pinMode(_btn, INPUT_PULLUP);
}

// Called from GPIO ISR on every CLK or DT CHANGE edge.
// Runs the state machine immediately so no transitions are missed.
void IRAM_ATTR Rotary::processISR() {
    uint8_t pinstate = (digitalRead(_clk) << 1) | digitalRead(_dt);
    _encState = TTABLE[_encState & 0x0f][pinstate];
    uint8_t dir = _encState & 0x30;
    if (dir) _pendingDir = dir;
}

// Returns accumulated direction since last call, then clears it.
uint8_t Rotary::consumePending() {
    uint8_t d = _pendingDir;
    _pendingDir = DIR_NONE;
    return d;
}

unsigned char Rotary::process_button() {
    bool pressed = (digitalRead(_btn) == LOW);  // active LOW

    if (!_btnDown && pressed) {
        _btnDown   = true;
        _longFired = false;
        _pressedAt = millis();
        return BTN_PRESSED;
    }
    if (_btnDown && !_longFired && (millis() - _pressedAt >= LONG_PRESS_MS)) {
        _longFired = true;
        return BTN_PRESSED_LONG;
    }
    if (_btnDown && !pressed) {
        _btnDown = false;
        return _longFired ? BTN_RELEASED_LONG : BTN_RELEASED;
    }
    return BTN_NONE;
}
