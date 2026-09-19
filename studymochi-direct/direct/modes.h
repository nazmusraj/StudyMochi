#pragma once

#include <Arduino.h>

#include "face.h"

enum MainMode : uint8_t {
  MODE_CLOCK = 0,
  MODE_TIMER = 1,
  MODE_AI = 2,
  MAIN_MODE_COUNT
};

enum SubClockMode : uint8_t {
  SUB_CLOCK_TIME = 0,
  SUB_WEATHER_NOW = 1,
  SUB_WEATHER_DETAILS = 2,
  SUB_WEATHER_TODAY = 3,
  SUB_CLOCK_COUNT
};

enum SubTimerMode : uint8_t {
  SUB_TIMER_POMO = 0,
  SUB_TIMER_CUSTOM = 1,
  SUB_TIMER_STOPWATCH = 2,
  SUB_TIMER_COUNT
};

// Stores navigation only. Touch, audio, timer actions, and pet reactions are
// dispatched centrally by direct.ino so an event can never be consumed twice.
class ModeManager {
public:
  void begin();
  bool setMainMode(MainMode mode);
  bool nextMainMode();
  bool nextSubMode();
  void refreshScreen();

  MainMode currentMainMode() const { return _mainMode; }
  uint8_t currentSubMode() const { return _subMode; }
  bool isAiMode() const { return _mainMode == MODE_AI; }

private:
  MainMode _mainMode = MODE_CLOCK;
  uint8_t _subMode = SUB_CLOCK_TIME;
};

extern ModeManager gModes;
