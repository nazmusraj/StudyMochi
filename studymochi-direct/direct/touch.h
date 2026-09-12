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
#define TOUCH_HOLD_MS     2000    // Default 2.0s hold threshold (e.g. mode change)

class Touch {
public:
  void begin(int pin, uint32_t holdMs = TOUCH_HOLD_MS);

  // Call periodically in loop(). Timestamp passed externally to allow
  // mocking millis() in desktop test environments.
  void update(uint32_t now);

  bool isDown() const { return _stable; }          // Current contact state
  uint32_t heldMs(uint32_t now) const;             // Contact duration in milliseconds

  void setHoldThreshold(uint32_t ms) { _holdThreshold = ms; }

  // ── Events: Returns true once, auto-clearing on read ──
  bool tookTap();        // Short tap (triggered on release)
  bool tookHold();       // Long press reached threshold (does not wait for release)
  bool tookRelease();    // Contact released
  bool tookRapidTap(uint8_t targetCount = 3, uint32_t windowMs = 1500); // N rapid taps within window

private:
  int      _pin           = -1;
  uint32_t _holdThreshold = TOUCH_HOLD_MS;
  bool     _raw           = false;
  bool     _stable        = false;
  uint32_t _changed       = 0;
  uint32_t _downAt        = 0;
  bool     _holdSent      = false;
  bool     _tap = false, _hold = false, _rel = false;

  // Multi-tap tracking for anger / rapid tap reactions
  static const uint8_t MAX_TAP_HISTORY = 6;
  uint32_t _tapHistory[MAX_TAP_HISTORY] = {0};
  uint8_t  _tapHistoryIdx = 0;
  bool     _rapidTap = false;
};

