#pragma once

#include <Arduino.h>

// Short notification patterns for a passive buzzer.  These sounds use a
// separate GPIO and never claim either I2S peripheral used by the microphone
// and MAX98357A amplifier.
enum BuzzerCue : uint8_t {
  BUZZ_BOOT,
  BUZZ_CLICK,
  BUZZ_MODE_CLOCK,
  BUZZ_MODE_TIMER,
  BUZZ_MODE_AI,
  BUZZ_MODE_POMO,
  BUZZ_ARM,
  BUZZ_START,
  BUZZ_PAUSE,
  BUZZ_RESET,
  BUZZ_DONE,
  BUZZ_BREAK,
  BUZZ_ERROR,
  BUZZ_HAPPY,
  BUZZ_ANGRY,
  BUZZ_SAD,
  BUZZ_DND_OFF
};

struct BuzzerStep {
  uint16_t frequency;
  uint16_t durationMs;
  uint16_t gapMs;
};

// Plays melodies incrementally from loop(); no delay() calls are used.
class BuzzerManager {
public:
  void begin(uint8_t pin);
  void tick(uint32_t now = millis());
  void play(BuzzerCue cue);
  void stop();
  void setEnabled(bool enabled);
  bool isBusy() const { return _steps != nullptr; }

private:
  void beginCurrentStep(uint32_t now);
  void advance(uint32_t now);

  uint8_t _pin = 255;
  bool _begun = false;
  bool _enabled = true;
  bool _toneOn = false;
  const BuzzerStep *_steps = nullptr;
  uint8_t _stepCount = 0;
  uint8_t _stepIndex = 0;
  uint32_t _deadline = 0;
};

extern BuzzerManager gBuzzer;
