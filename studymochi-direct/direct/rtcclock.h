// ════════════════════════════════════════════════════════════════
//   DS3231 RTC Clock — Preserves time across power cuts with backup battery.
//
//   Wiring — Shared on the SAME I2C bus as the OLED, requiring no extra pins:
//       VCC → 3V3      GND → GND      SDA → GPIO 21     SCL → GPIO 22
//   (DS3231 address 0x68, OLED address 0x3C — no address conflict)
//
//   Time is synchronized in two ways:
//     1) Via NTP when WiFi is connected — highly accurate, automatic
//     2) Fallback to firmware compilation time when NTP is unavailable
//
//   Requires RTClib library (Adafruit).
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

struct MochiTime {
  int  hour24 = 0, minute = 0, second = 0;
  int  day = 1, month = 1, year = 2026;
  int  dow = 0;              // 0 = Sunday
  bool valid = false;
};

// Initializes and verifies RTC. Returns false if not detected;
// firmware continues executing, but clock screen displays "No RTC".
bool clockBegin();
bool clockOk();

MochiTime clockNow();

// Call once WiFi connection is established. Synchronizes time from NTP
// into the DS3231 if drift exceeds 2 seconds. Returns true on success.
bool clockSyncNTP(long gmtOffsetSec = 6 * 3600);   // Bangladesh = UTC+6

// Sets RTC to build/compile timestamp — last resort if NTP is unavailable
void clockSetFromBuild();

// Converts integer to Bengali numerals in UTF-8 (e.g. "11:34" in Bengali digits)
String banglaDigits(int n, int pad = 0);
const char *banglaDayName(int dow);       // e.g. Bengali name for Friday
const char *banglaMonthName(int m);       // e.g. Bengali name for September

