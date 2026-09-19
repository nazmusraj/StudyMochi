#pragma once

#include <Arduino.h>

// The DS3231 keeps local time through power loss. SNTP corrects the ESP32 and
// DS3231 in the background whenever Wi-Fi is available.
struct MochiTime {
  int hour24 = 0;
  int minute = 0;
  int second = 0;
  int day = 1;
  int month = 1;
  int year = 2026;
  int dow = 0;  // 0 = Sunday
  bool valid = false;
};

bool clockBegin(long storedUtcOffsetSec = 6 * 3600);
bool clockOk();
MochiTime clockNow();

// Starts or updates non-blocking SNTP synchronization. clockUpdate() performs
// deferred DS3231 writes in the Arduino loop instead of a network callback.
void clockConfigureNtp(long utcOffsetSec = 6 * 3600);
void clockUpdate(uint32_t now, bool wifiConnected);
bool clockTookNtpSync();
long clockUtcOffset();

// Compatibility helper for older call sites. It no longer blocks.
bool clockSyncNTP(long gmtOffsetSec = 6 * 3600);

void clockSetFromBuild();

String banglaDigits(int n, int pad = 0);
const char *banglaDayName(int dow);
const char *banglaMonthName(int month);
