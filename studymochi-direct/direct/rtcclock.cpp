#include "rtcclock.h"
#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include <sys/time.h>

static RTC_DS3231 rtc;
static bool gHasRtc = false;

bool clockBegin() {
  // Wire.begin() is already called in faceBegin()
  gHasRtc = rtc.begin(&Wire);
  if (!gHasRtc) {
    Serial.println("[rtc] DS3231 not detected on I2C (0x68). Using ESP32 internal RTC.");
  } else {
    Serial.println("[rtc] DS3231 Hardware RTC detected (0x68) OK.");
    if (rtc.lostPower()) {
      Serial.println("[rtc] DS3231 battery was disconnected - needs NTP sync.");
    } else {
      DateTime cur = rtc.now();
      if (cur.year() >= 2024) {
        struct timeval tv;
        tv.tv_sec = cur.unixtime();
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
        Serial.printf("[rtc] Restored time from DS3231: %02d:%02d:%02d\n",
                      cur.hour(), cur.minute(), cur.second());
      }
    }
  }

  // Pre-configure NTP timezone (UTC+6 Bangladesh default: 6 * 3600s)
  configTime(6 * 3600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  // If time is still at epoch 1970, initialize from compile timestamp
  time_t nowSec = time(nullptr);
  if (nowSec < 1700000000) {
    clockSetFromBuild();
  }
  return true;
}

bool clockOk() {
  MochiTime t = clockNow();
  return t.valid;
}

MochiTime clockNow() {
  MochiTime t;
  struct tm tmNow;

  if (getLocalTime(&tmNow, 20)) {
    t.hour24 = tmNow.tm_hour;
    t.minute = tmNow.tm_min;
    t.second = tmNow.tm_sec;
    t.day    = tmNow.tm_mday;
    t.month  = tmNow.tm_mon + 1;
    t.year   = tmNow.tm_year + 1900;
    t.dow    = tmNow.tm_wday;
    t.valid  = (t.year >= 2024);
    return t;
  }

  // Fallback to hardware DS3231 if internal time failed to read
  if (gHasRtc) {
    DateTime n = rtc.now();
    t.hour24 = n.hour();
    t.minute = n.minute();
    t.second = n.second();
    t.day    = n.day();
    t.month  = n.month();
    t.year   = n.year();
    t.dow    = n.dayOfTheWeek();
    t.valid  = (n.year() >= 2024);
    return t;
  }

  return t;
}

void clockSetFromBuild() {
  DateTime built(F(__DATE__), F(__TIME__));
  if (gHasRtc) {
    rtc.adjust(built);
  }
  struct timeval tv;
  tv.tv_sec = built.unixtime();
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  Serial.printf("[rtc] Set baseline time from build: %02d:%02d:%02d %02d/%02d/%04d\n",
                built.hour(), built.minute(), built.second(),
                built.day(), built.month(), built.year());
}

bool clockSyncNTP(long gmtOffsetSec) {
  configTime(gmtOffsetSec, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  struct tm tmNow;
  // Try up to 10 checks (each waiting up to 300ms)
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&tmNow, 300)) {
      if (gHasRtc) {
        DateTime net(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                     tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
        rtc.adjust(net);
        Serial.println("[rtc] DS3231 RTC synchronized with NTP");
      }
      Serial.printf("[rtc] NTP sync SUCCESS: %02d:%02d:%02d  %02d/%02d/%04d\n",
                    tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec,
                    tmNow.tm_mday, tmNow.tm_mon + 1, tmNow.tm_year + 1900);
      return true;
    }
    delay(50);
  }
  Serial.println("[rtc] NTP server not reachable yet");
  return false;
}

// ───────────────────── Bengali Text Helpers ─────────────────────
static const char *BN_DIGIT[10] = {
  "০", "১", "২", "৩", "৪", "৫", "৬", "৭", "৮", "৯"
};

String banglaDigits(int n, int pad) {
  bool neg = n < 0;
  if (neg) n = -n;
  String eng = String(n);
  while ((int)eng.length() < pad) eng = "0" + eng;
  String out;
  for (size_t i = 0; i < eng.length(); i++) {
    char c = eng[i];
    if (c >= '0' && c <= '9') out += BN_DIGIT[c - '0'];
    else out += c;
  }
  return neg ? ("-" + out) : out;
}

const char *banglaDayName(int dow) {
  static const char *D[7] = {
    "রবিবার", "সোমবার", "মঙ্গলবার", "বুধবার",
    "বৃহস্পতিবার", "শুক্রবার", "শনিবার"
  };
  return D[(dow % 7 + 7) % 7];
}

const char *banglaMonthName(int m) {
  static const char *M[12] = {
    "জানুয়ারি", "ফেব্রুয়ারি", "মার্চ", "এপ্রিল", "মে", "জুন",
    "জুলাই", "আগস্ট", "সেপ্টেম্বর", "অক্টোবর", "নভেম্বর", "ডিসেম্বর"
  };
  return M[(m >= 1 && m <= 12) ? (m - 1) : 0];
}
