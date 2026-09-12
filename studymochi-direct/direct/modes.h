// ════════════════════════════════════════════════════════════════
//   Mode Manager for StudyMochi v2
//
//   Central navigation state machine:
//     Main Modes:
//       1. MODE_CLOCK (Sub 0: Clock/Date/Day, Sub 1: Weather)
//       2. MODE_TIMER (Sub 0: Pomodoro, Sub 1: Custom Timer, Sub 2: Stopwatch)
//       3. MODE_AI    (Gemini Live conversational assistant)
//
//     Interactions:
//       Side Touch LONG PRESS (2s) -> Cycle Main Mode
//       Side Touch SHORT TAP       -> Cycle Sub Mode
//       Top Touch                  -> Pet interaction / Context actions
//       Orientation Change         -> Pomodoro preset select / Face Squish
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
#include "face.h"
#include "touch.h"
#include "imu.h"
#include "dfvoice.h"
#include "mood.h"
#include "pomodoro_engine.h"
#include "stopwatch.h"

enum MainMode {
  MODE_CLOCK = 0,
  MODE_TIMER = 1,
  MODE_AI    = 2,
  MAIN_MODE_COUNT
};

enum SubClockMode {
  SUB_CLOCK_TIME    = 0,
  SUB_CLOCK_WEATHER = 1,
  SUB_CLOCK_COUNT
};

enum SubTimerMode {
  SUB_TIMER_POMO      = 0,
  SUB_TIMER_CUSTOM    = 1,
  SUB_TIMER_STOPWATCH = 2,
  SUB_TIMER_COUNT
};

class ModeManager {
public:
  void begin();
  void update(uint32_t now, Touch &topTouch, Touch &sideTouch);

  MainMode currentMainMode() const { return _mainMode; }
  uint8_t  currentSubMode() const  { return _subMode; }

  void nextMainMode();
  void nextSubMode();
  void setMainMode(MainMode m);

  // Status queries
  bool isAiMode() const { return (_mainMode == MODE_AI); }

private:
  MainMode _mainMode = MODE_CLOCK;
  uint8_t  _subMode  = 0;

  void applyScreen();
  void handleTopTouch(Touch &topTouch);
  void handleOrientation();
};

extern ModeManager gModes;
