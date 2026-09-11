#include "touch.h"

void Touch::begin(int pin) {
  _pin = pin;
  // TTP223 নিজেই শক্ত করে HIGH/LOW দেয়, তাই pull-up/down লাগে না।
  // তবু INPUT_PULLDOWN দিলে তার খুলে গেলে "সবসময় ছোঁয়া" হয়ে যায় না।
#if TOUCH_ACTIVE_LOW
  pinMode(_pin, INPUT_PULLUP);
#else
  pinMode(_pin, INPUT_PULLDOWN);
#endif
  _raw = _stable = false;
  _changed = _downAt = 0;
  _holdSent = false;
  _tap = _hold = _rel = false;
}

void Touch::update(uint32_t now) {
  if (_pin < 0) return;

  int v = digitalRead(_pin);
#if TOUCH_ACTIVE_LOW
  bool raw = (v == LOW);
#else
  bool raw = (v == HIGH);
#endif

  // ── ঝাঁকুনি সরাই ──
  // আঙুল ছোঁয়ানোর মুহূর্তে সংকেত কয়েকবার লাফায়। তাই মান বদলালে
  // সাথে সাথে বিশ্বাস করি না — TOUCH_DEBOUNCE_MS ধরে একই থাকলে
  // তবেই "সত্যি বদলেছে" ধরি।
  if (raw != _raw) {
    _raw = raw;
    _changed = now;
    return;
  }
  if (raw == _stable) {
    // অবস্থা বদলায়নি — শুধু চেপে ধরা হয়েছে কি না দেখি
    if (_stable && !_holdSent && (now - _downAt) >= TOUCH_HOLD_MS) {
      _holdSent = true;
      _hold = true;
    }
    return;
  }
  if (now - _changed < TOUCH_DEBOUNCE_MS) return;

  // ── সত্যিকারের বদল ──
  _stable = raw;
  if (_stable) {
    _downAt = now;
    _holdSent = false;
  } else {
    _rel = true;
    // চেপে ধরা হয়ে থাকলে সেটা আর "ট্যাপ" নয়
    if (!_holdSent) _tap = true;
  }
}

uint32_t Touch::heldMs(uint32_t now) const {
  return _stable ? (now - _downAt) : 0;
}

bool Touch::tookTap()     { bool v = _tap;  _tap  = false; return v; }
bool Touch::tookHold()    { bool v = _hold; _hold = false; return v; }
bool Touch::tookRelease() { bool v = _rel;  _rel  = false; return v; }
