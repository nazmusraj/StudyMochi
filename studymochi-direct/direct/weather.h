// ════════════════════════════════════════════════════════════════
//   Weather Client — Using Open-Meteo.
//
//   Why Open-Meteo: Requires NO API key, is free, and has generous
//   limits for personal projects. Unlike services such as OpenWeatherMap,
//   there is no need to register or manage keys — a single HTTPS GET suffices.
//
//   Default location: Dhaka (23.81, 90.41). Configurable via portal.
//
//   No external dependencies: Uses WiFiClientSecure directly for GET,
//   and parses the compact response with a lightweight parser
//   (no need for ArduinoJson).
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

struct WeatherNow {
  float   tempC     = 0;
  int     humidity  = 0;
  int     code      = -1;      // WMO weather code
  float   windKmh   = 0;
  bool    valid     = false;
  uint32_t fetchedAt = 0;      // millis()
};

// Fetches weather data from network. Returns cached data if fetched
// within the last 15 minutes (set force = true to force a fresh request).
bool weatherFetch(float lat, float lon, bool force = false);

WeatherNow weatherGet();

// Maps WMO numeric weather code to Bengali description (e.g. "Drizzle")
const char *weatherBangla(int code);

// Generates the spoken weather prompt sentence for Mochi
String weatherSentence(const WeatherNow &w);
