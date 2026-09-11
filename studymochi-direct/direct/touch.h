// ════════════════════════════════════════════════════════════════
//   TTP223 Touch Sensor — Distinguishes touches, taps, and long presses.
//
//   Wiring (3 pins per module):
//       VCC → 3V3      GND → GND      OUT/SIG → GPIO
//
//   TTP223 defaults to active HIGH when touched. Some modules allow
//   inverting this by bridging solder pads A/B on the back — if inverted,
//   set TOUCH_ACTIVE_LOW to 1.
//
//   Why a dedicated file: Encapsulating button logic (debouncing, tap vs.
//   hold detection) keeps direct.ino clean and facilitates PC unit testing.
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

// Set to 1 if the module output is active LOW
#ifndef TOUCH_ACTIVE_LOW
#define TOUCH_ACTIVE_LOW 0
#endif

#define TOUCH_DEBOUNCE_MS 40      // Debounce window to filter electrical noise
#define TOUCH_HOLD_MS     600     // Duration after which a touch is registered as a "hold"

class Touch {
public:
  void begin(int pin);

  // Call periodically in loop(). Timestamp passed externally to allow
  // mocking millis() in desktop test environments.
  void update(uint32_t now);

  bool isDown() const { return _stable; }          // Current contact state
  uint32_t heldMs(uint32_t now) const;             // Contact duration in milliseconds

  // ── Events: Returns true once, auto-clearing on read ──
  bool tookTap();        // Short tap (triggered on release)
  bool tookHold();       // Long press started (does not wait for release)
  bool tookRelease();    // Contact released

private:
  int      _pin      = -1;
  bool     _raw      = false;
  bool     _stable   = false;
  uint32_t _changed  = 0;
  uint32_t _downAt   = 0;
  bool     _holdSent = false;
  bool     _tap = false, _hold = false, _rel = false;
};
