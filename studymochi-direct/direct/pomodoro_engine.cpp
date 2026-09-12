#include "pomodoro_engine.h"

PomodoroEngine gPomodoro;

void PomodoroEngine::begin() {
  selectProfile(0);
}

void PomodoroEngine::selectProfile(int index) {
  if (index < 0 || index >= POMO_NUM_PROFILES) index = 0;
  if (_profileIdx == index && _phase != POMO_PHASE_IDLE) return;

  _profileIdx = index;
  reset();
}

void PomodoroEngine::nextProfile() {
  selectProfile((_profileIdx + 1) % POMO_NUM_PROFILES);
}

void PomodoroEngine::reset() {
  _phase = POMO_PHASE_IDLE;
  _round = 1;
  _totalSec = POMO_PROFILES[_profileIdx].workSec;
  _remainingSec = _totalSec;
  _lastTick = millis();
}

void PomodoroEngine::start() {
  if (_phase == POMO_PHASE_IDLE || _phase == POMO_PHASE_PAUSED) {
    if (_phase == POMO_PHASE_IDLE) {
      _totalSec = POMO_PROFILES[_profileIdx].workSec;
      _remainingSec = _totalSec;
    }
    _phase = POMO_PHASE_WORK;
    _lastTick = millis();
    _phaseChanged = true;
  }
}

void PomodoroEngine::pause() {
  if (isRunning()) {
    _phase = POMO_PHASE_PAUSED;
  }
}

void PomodoroEngine::toggle() {
  if (isRunning()) {
    pause();
  } else {
    start();
  }
}

void PomodoroEngine::tick(uint32_t now) {
  if (!isRunning()) {
    _lastTick = now;
    return;
  }

  if (now - _lastTick >= 1000) {
    uint32_t elapsedSec = (now - _lastTick) / 1000;
    _lastTick = now;

    if (_remainingSec > elapsedSec) {
      _remainingSec -= elapsedSec;
    } else {
      _remainingSec = 0;

      // Phase Transition
      if (_phase == POMO_PHASE_WORK) {
        if (_round >= POMO_PROFILES[_profileIdx].roundsBeforeLong) {
          _phase = POMO_PHASE_LONG_BREAK;
          _totalSec = POMO_PROFILES[_profileIdx].longBreakSec;
          _round = 1; // Reset rounds
        } else {
          _phase = POMO_PHASE_BREAK;
          _totalSec = POMO_PROFILES[_profileIdx].breakSec;
          _round++;
        }
      } else { // After Break or Long Break
        _phase = POMO_PHASE_WORK;
        _totalSec = POMO_PROFILES[_profileIdx].workSec;
      }

      _remainingSec = _totalSec;
      _phaseChanged = true;
    }
  }
}

float PomodoroEngine::progress() const {
  if (_totalSec == 0) return 0.0f;
  return 1.0f - ((float)_remainingSec / (float)_totalSec);
}

bool PomodoroEngine::tookPhaseChange(PomoPhase &newPhase) {
  if (_phaseChanged) {
    _phaseChanged = false;
    newPhase = _phase;
    return true;
  }
  return false;
}
