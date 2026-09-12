// ════════════════════════════════════════════════════════════════
//   Lightweight Stopwatch Module for StudyMochi
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

class Stopwatch {
public:
  void begin();
  void tick(uint32_t now);

  void start();
  void stop();
  void toggle();
  void reset();

  bool isRunning() const { return _running; }
  uint32_t elapsedMs() const;

  // Components for display
  uint32_t minutes() const;
  uint32_t seconds() const;
  uint32_t centis() const; // 1/100ths of a second (00-99)

private:
  bool     _running    = false;
  uint32_t _startMs    = 0;
  uint32_t _accumMs    = 0;
  uint32_t _currentNow = 0;
};

extern Stopwatch gStopwatch;
