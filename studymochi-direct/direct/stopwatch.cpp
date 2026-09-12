#include "stopwatch.h"

Stopwatch gStopwatch;

void Stopwatch::begin() {
  reset();
}

void Stopwatch::tick(uint32_t now) {
  _currentNow = now;
}

void Stopwatch::start() {
  if (!_running) {
    _running = true;
    _startMs = millis();
  }
}

void Stopwatch::stop() {
  if (_running) {
    _accumMs += (millis() - _startMs);
    _running = false;
  }
}

void Stopwatch::toggle() {
  if (_running) stop();
  else start();
}

void Stopwatch::reset() {
  _running = false;
  _accumMs = 0;
  _startMs = 0;
}

uint32_t Stopwatch::elapsedMs() const {
  if (_running) {
    return _accumMs + (millis() - _startMs);
  }
  return _accumMs;
}

uint32_t Stopwatch::minutes() const {
  return (elapsedMs() / 60000);
}

uint32_t Stopwatch::seconds() const {
  return (elapsedMs() % 60000) / 1000;
}

uint32_t Stopwatch::centis() const {
  return (elapsedMs() % 1000) / 10;
}
