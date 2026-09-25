#include "modes.h"

ModeManager gModes;

void ModeManager::begin() {
  _mainMode = MODE_CLOCK;
  _subMode = SUB_CLOCK_TIME;
  refreshScreen();
}
bool ModeManager::setMainMode(MainMode mode) {
  if (mode >= MAIN_MODE_COUNT) mode = MODE_CLOCK;
  if (_mainMode == mode && _subMode == 0) return false;
  _mainMode = mode;
  _subMode = 0;
  refreshScreen();
  return true;
}

bool ModeManager::nextMainMode() {
  return setMainMode((MainMode)((_mainMode + 1) % MAIN_MODE_COUNT));
}

bool ModeManager::nextSubMode() {
  if (_mainMode == MODE_CLOCK) {
    _subMode = (_subMode + 1) % SUB_CLOCK_COUNT;
  } else if (_mainMode == MODE_TIMER) {
    _subMode = (_subMode + 1) % SUB_TIMER_COUNT;
  } else {
    return false;
  }
  refreshScreen();
  return true;
}

void ModeManager::refreshScreen() {
  if (_mainMode == MODE_AI) {
    faceSetScreen(SCR_FACE);
    return;
  }
  if (_mainMode == MODE_PET_POMO) {
    faceSetScreen(SCR_POMO);
    return;
  }
  if (_mainMode == MODE_CLOCK) {
    static const FaceScreen screens[SUB_CLOCK_COUNT] = {
      SCR_CLOCK, SCR_WEATHER_NOW, SCR_WEATHER_DETAILS, SCR_WEATHER_TODAY
    };
    faceSetScreen(screens[_subMode % SUB_CLOCK_COUNT]);
    return;
  }
  static const FaceScreen timerScreens[SUB_TIMER_COUNT] = {
    SCR_TIMER, SCR_STOPWATCH
  };
  faceSetScreen(timerScreens[_subMode % SUB_TIMER_COUNT]);
}
