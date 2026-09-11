#include "rtcclock.h"
#include <Wire.h>
#include <RTClib.h>
#include <time.h>

static RTC_DS3231 rtc;
static bool gOk = false;

bool clockBegin() {
  // Wire.begin() is already called in face.cpp — do not call it again here,
  // otherwise OLED frequency settings (400 kHz) could be overwritten.
  gOk = rtc.begin(&Wire);
  if (!gOk) {
    Serial.println("[rtc] DS3231 pai ni (0x68). tar dekhun: SDA 21, SCL 22");
    return false;
  }
  Serial.println("[rtc] DS3231 OK");
  if (rtc.lostPower())
    Serial.println("[rtc] battery giyechilo — somoy bosate hobe");
  return true;
}

bool clockOk() { return gOk; }

MochiTime clockNow() {
  MochiTime t;
  if (!gOk) return t;
  DateTime n = rtc.now();
  t.hour24 = n.hour(); t.minute = n.minute(); t.second = n.second();
  t.day = n.day(); t.month = n.month(); t.year = n.year();
  t.dow = n.dayOfTheWeek();
  t.valid = n.year() >= 2024;      // If year is before 2024, time was never initialized
  return t;
}

void clockSetFromBuild() {
  if (!gOk) return;
  DateTime built(F(__DATE__), F(__TIME__));
  rtc.adjust(built);
  Serial.println("[rtc] compile-er somoy bosalam (NTP na pele ei-ta)");
}

bool clockSyncNTP(long gmtOffsetSec) {
  if (!gOk) return false;
  configTime(gmtOffsetSec, 0, "pool.ntp.org", "time.google.com");

  struct tm tmNow;
  // Wait ~8 seconds; abort if NTP is unreachable
  for (int i = 0; i < 16; i++) {
    if (getLocalTime(&tmNow, 500)) {
      DateTime net(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                   tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
      DateTime cur = rtc.now();
      long diff = (long)net.unixtime() - (long)cur.unixtime();
      if (diff < 0) diff = -diff;
      if (diff > 2 || rtc.lostPower()) {
        rtc.adjust(net);
        Serial.printf("[rtc] NTP theke somoy bosalam (%ld s foraq chhilo)\n", diff);
      } else {
        Serial.println("[rtc] NTP-r sathe milche, bodlanor dorkar nei");
      }
      return true;
    }
  }
  Serial.println("[rtc] NTP pelam na");
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
  if (m < 1 || m > 12) return "";
  return M[m - 1];
}
