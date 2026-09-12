// ════════════════════════════════════════════════════════════════
//   Extensible Pomodoro Engine for StudyMochi
//
//   Allows adding any custom Pomodoro preset.
//   Supports 4-orientation switching, work/break/long-break cycles,
//   and phase transition events.
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

enum PomoPhase {
  POMO_PHASE_IDLE       = 0,
  POMO_PHASE_WORK       = 1,
  POMO_PHASE_BREAK      = 2,
  POMO_PHASE_LONG_BREAK = 3,
  POMO_PHASE_PAUSED     = 4
};

struct PomoProfile {
  const char* id;           // Short ID
  const char* label;        // e.g. "25-5"
  const char* title;        // e.g. "Classic"
  uint16_t    workSec;      // Work duration in seconds
  uint16_t    breakSec;     // Short break in seconds
  uint16_t    longBreakSec; // Long break in seconds
  uint8_t     roundsBeforeLong; // Completed work rounds before long break
};

// ── Default 4 Popular Presets (can be expanded) ──
static const PomoProfile POMO_PROFILES[] = {
  { "CLASSIC",   "25-5",  "Classic",   25 * 60,  5 * 60, 15 * 60, 4 },
  { "DEEP",      "50-10", "Deep Work", 50 * 60, 10 * 60, 20 * 60, 4 },
  { "SPRINT",    "15-3",  "Sprint",    15 * 60,  3 * 60, 10 * 60, 4 },
  { "ULTRADIAN", "90-20", "Extended",  90 * 60, 20 * 60, 30 * 60, 3 },
};
static const int POMO_NUM_PROFILES = sizeof(POMO_PROFILES) / sizeof(POMO_PROFILES[0]);

class PomodoroEngine {
public:
  void begin();
  void tick(uint32_t now);

  // Control
  void start();
  void pause();
  void toggle();
  void reset();

  // Profile selection
  void selectProfile(int index);
  void nextProfile();
  int  currentProfileIndex() const { return _profileIdx; }
  const PomoProfile& currentProfile() const { return POMO_PROFILES[_profileIdx]; }

  // State queries
  PomoPhase phase() const { return _phase; }
  bool isRunning() const { return (_phase == POMO_PHASE_WORK || _phase == POMO_PHASE_BREAK || _phase == POMO_PHASE_LONG_BREAK); }
  uint32_t remainingSec() const { return _remainingSec; }
  uint32_t totalSec() const { return _totalSec; }
  uint8_t currentRound() const { return _round; }
  float progress() const; // 0.0 to 1.0

  // Event queries
  bool tookPhaseChange(PomoPhase &newPhase);

private:
  int       _profileIdx = 0;
  PomoPhase _phase = POMO_PHASE_IDLE;
  PomoPhase _prevPhase = POMO_PHASE_IDLE;
  bool      _phaseChanged = false;

  uint32_t  _remainingSec = 25 * 60;
  uint32_t  _totalSec     = 25 * 60;
  uint32_t  _lastTick     = 0;
  uint8_t   _round        = 1;
};

extern PomodoroEngine gPomodoro;
