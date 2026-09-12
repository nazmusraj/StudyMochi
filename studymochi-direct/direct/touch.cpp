#include "touch.h"

void Touch::begin(int pin, uint32_t holdMs) {
  _pin = pin;
  _holdThreshold = holdMs;
  // TTP223 actively drives output HIGH/LOW, so external pull-up/down is not required.
  // However, configuring INPUT_PULLDOWN prevents floating false-positives if a wire disconnects.
#if TOUCH_ACTIVE_LOW
  pinMode(_pin, INPUT_PULLUP);
#else
  pinMode(_pin, INPUT_PULLDOWN);
#endif
  _raw = _stable = false;
  _changed = _downAt = 0;
  _holdSent = false;
  _tap = _hold = _rel = false;
  _rapidTap = false;
  for (int i = 0; i < MAX_TAP_HISTORY; i++) _tapHistory[i] = 0;
  _tapHistoryIdx = 0;
}

void Touch::update(uint32_t now) {
  if (_pin < 0) return;

  int v = digitalRead(_pin);
#if TOUCH_ACTIVE_LOW
  bool raw = (v == LOW);
#else
  bool raw = (v == HIGH);
#endif

  // ── Debounce Filtering ──
  if (raw != _raw) {
    _raw = raw;
    _changed = now;
    return;
  }
  if (raw == _stable) {
    // State unchanged — check if hold duration threshold is reached
    if (_stable && !_holdSent && (now - _downAt) >= _holdThreshold) {
      _holdSent = true;
      _hold = true;
    }
    return;
  }
  if (now - _changed < TOUCH_DEBOUNCE_MS) return;

  // ── Valid State Transition ──
  _stable = raw;
  if (_stable) {
    _downAt = now;
    _holdSent = false;
  } else {
    _rel = true;
    // If a hold was already dispatched, do not register a tap on release
    if (!_holdSent) {
      _tap = true;
      // Record tap timestamp in circular buffer
      _tapHistory[_tapHistoryIdx] = now;
      _tapHistoryIdx = (_tapHistoryIdx + 1) % MAX_TAP_HISTORY;

      // Check if rapid tap threshold met (3 taps within 1500 ms)
      uint8_t count = 0;
      for (int i = 0; i < MAX_TAP_HISTORY; i++) {
        if (_tapHistory[i] > 0 && (now - _tapHistory[i]) <= 1500) {
          count++;
        }
      }
      if (count >= 3) {
        _rapidTap = true;
        // Clear history so we don't immediately re-trigger without fresh taps
        for (int i = 0; i < MAX_TAP_HISTORY; i++) _tapHistory[i] = 0;
      }
    }
  }
}

uint32_t Touch::heldMs(uint32_t now) const {
  return _stable ? (now - _downAt) : 0;
}

bool Touch::tookTap() {
  bool v = _tap;
  _tap = false;
  return v;
}

bool Touch::tookHold() {
  bool v = _hold;
  _hold = false;
  return v;
}

bool Touch::tookRelease() {
  bool v = _rel;
  _rel = false;
  return v;
}

bool Touch::tookRapidTap(uint8_t targetCount, uint32_t windowMs) {
  bool v = _rapidTap;
  _rapidTap = false;
  return v;
}

