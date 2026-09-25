#include "buzzer.h"

BuzzerManager gBuzzer;

namespace {
const BuzzerStep BOOT[] = {
    {1050, 80, 30}, {1450, 80, 30}, {1950, 120, 0}};
const BuzzerStep CLICK[] = {{1850, 45, 0}};
const BuzzerStep MODE_CLOCK[] = {{900, 100, 45}, {1200, 150, 0}};
const BuzzerStep MODE_TIMER[] = {{1250, 100, 45}, {1250, 150, 0}};
const BuzzerStep MODE_AI[] = {{1200, 100, 45}, {1750, 150, 0}};
const BuzzerStep MODE_POMO[] = {
    {1000, 80, 35}, {1400, 80, 35}, {1900, 140, 0}};
const BuzzerStep ARM[] = {{1550, 100, 0}};
const BuzzerStep START[] = {{1200, 100, 45}, {1850, 160, 0}};
const BuzzerStep PAUSE[] = {{1500, 100, 45}, {950, 160, 0}};
const BuzzerStep RESET[] = {{750, 160, 0}};
const BuzzerStep DONE[] = {
    {1250, 150, 50}, {1650, 150, 50}, {2150, 300, 0}};
const BuzzerStep BREAK[] = {{1800, 140, 60}, {1150, 220, 0}};
const BuzzerStep ERROR[] = {{360, 160, 70}, {360, 200, 0}};
const BuzzerStep HAPPY[] = {
    {1500, 120, 40}, {2000, 160, 40}, {2500, 250, 0}};
const BuzzerStep ANGRY[] = {
    {430, 160, 60}, {330, 200, 60}, {430, 280, 0}};
const BuzzerStep SAD[] = {
    {900, 220, 80}, {650, 300, 80}, {450, 450, 0}};
const BuzzerStep DND_OFF[] = {{800, 100, 45}, {1400, 180, 0}};

template <size_t N>
void choose(const BuzzerStep (&pattern)[N], const BuzzerStep *&steps,
            uint8_t &count) {
  steps = pattern;
  count = (uint8_t)N;
}
} // namespace

void BuzzerManager::begin(uint8_t pin) {
  _pin = pin;
  _begun = true;
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
}

void BuzzerManager::setEnabled(bool enabled) {
  _enabled = enabled;
  if (!enabled)
    stop();
}

void BuzzerManager::stop() {
  if (_begun && _toneOn)
    noTone(_pin);
  if (_begun)
    digitalWrite(_pin, LOW);
  _toneOn = false;
  _steps = nullptr;
  _stepCount = 0;
  _stepIndex = 0;
  _deadline = 0;
}

void BuzzerManager::play(BuzzerCue cue) {
  if (!_begun || !_enabled)
    return;

  stop();
  switch (cue) {
  case BUZZ_BOOT:       choose(BOOT, _steps, _stepCount); break;
  case BUZZ_CLICK:      choose(CLICK, _steps, _stepCount); break;
  case BUZZ_MODE_CLOCK: choose(MODE_CLOCK, _steps, _stepCount); break;
  case BUZZ_MODE_TIMER: choose(MODE_TIMER, _steps, _stepCount); break;
  case BUZZ_MODE_AI:    choose(MODE_AI, _steps, _stepCount); break;
  case BUZZ_MODE_POMO:  choose(MODE_POMO, _steps, _stepCount); break;
  case BUZZ_ARM:        choose(ARM, _steps, _stepCount); break;
  case BUZZ_START:      choose(START, _steps, _stepCount); break;
  case BUZZ_PAUSE:      choose(PAUSE, _steps, _stepCount); break;
  case BUZZ_RESET:      choose(RESET, _steps, _stepCount); break;
  case BUZZ_DONE:       choose(DONE, _steps, _stepCount); break;
  case BUZZ_BREAK:      choose(BREAK, _steps, _stepCount); break;
  case BUZZ_ERROR:      choose(ERROR, _steps, _stepCount); break;
  case BUZZ_HAPPY:      choose(HAPPY, _steps, _stepCount); break;
  case BUZZ_ANGRY:      choose(ANGRY, _steps, _stepCount); break;
  case BUZZ_SAD:        choose(SAD, _steps, _stepCount); break;
  case BUZZ_DND_OFF:    choose(DND_OFF, _steps, _stepCount); break;
  }

  _stepIndex = 0;
  beginCurrentStep(millis());
}

void BuzzerManager::beginCurrentStep(uint32_t now) {
  if (!_steps || _stepIndex >= _stepCount) {
    stop();
    return;
  }
  // Give the ESP32 tone task the duration as a hardware-level safety limit.
  // If Wi-Fi or an HTTP request temporarily blocks loop(), the buzzer still
  // stops instead of holding one note until control returns here.
  tone(_pin, _steps[_stepIndex].frequency,
       _steps[_stepIndex].durationMs);
  _toneOn = true;
  _deadline = now + _steps[_stepIndex].durationMs;
}

void BuzzerManager::advance(uint32_t now) {
  if (_toneOn) {
    noTone(_pin);
    digitalWrite(_pin, LOW);
    _toneOn = false;
    uint16_t gap = _steps[_stepIndex].gapMs;
    if (gap) {
      _deadline = now + gap;
      return;
    }
  }

  _stepIndex++;
  if (_stepIndex >= _stepCount) {
    stop();
    return;
  }
  beginCurrentStep(now);
}

void BuzzerManager::tick(uint32_t now) {
  if (!_steps || (int32_t)(now - _deadline) < 0)
    return;
  advance(now);
}
