#pragma once

#include <Arduino.h>

// Cached Open-Meteo values used by the three low-resolution weather pages.
struct WeatherNow {
  float temperatureC = 0.0f;
  float apparentC = 0.0f;
  float windKmh = 0.0f;
  float maximumC = 0.0f;
  float minimumC = 0.0f;
  int humidity = 0;
  int weatherCode = -1;
  int rainProbability = 0;
  int utcOffsetSeconds = 6 * 3600;
  bool isDay = true;
  bool valid = false;
  uint32_t fetchedAt = 0;
};

// Fetches one compact forecast from Open-Meteo. Cached data remains usable
// during Wi-Fi loss and is refreshed no more than once every 15 minutes.
bool weatherFetch(float latitude, float longitude, bool force = false);
WeatherNow weatherGet();
bool weatherNeedsRefresh(uint32_t now);

// Bengali strings are device output. Code, diagnostics, and documentation
// remain English.
const char *weatherBangla(int code);
String weatherSentence(const WeatherNow &weather);
