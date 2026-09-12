#include "rtcclock.h"
#include <Wire.h>
#include <time.h>
#include <sys/time.h>

#define DS3231_I2C_ADDR 0x68

static bool gHasRtc = false;

// ───────────────────── BCD Helpers ─────────────────────
static inline uint8_t bcd2dec(uint8_t val) {
  return ((val >> 4) * 10) + (val & 0x0F);
}

static inline uint8_t dec2bcd(uint8_t val) {
  return ((val / 10) << 4) | (val % 10);
}

// Read raw registers from DS3231 via I2C without external library
static bool ds3231Read(MochiTime &t) {
  Wire.beginTransmission(DS3231_I2C_ADDR);
  Wire.write(0x00); // Start at register 00h (seconds)
  if (Wire.endTransmission() != 0) return false;

  if (Wire.requestFrom(DS3231_I2C_ADDR, 7) != 7) return false;

  t.second = bcd2dec(Wire.read() & 0x7F);
  t.minute = bcd2dec(Wire.read() & 0x7F);

  uint8_t rawHour = Wire.read();
  if (rawHour & 0x40) { // 12-hour mode
    t.hour24 = bcd2dec(rawHour & 0x1F);
    if (rawHour & 0x20) t.hour24 = (t.hour24 % 12) + 12; // PM
    else if (t.hour24 == 12) t.hour24 = 0;                 // 12 AM
  } else {             // 24-hour mode
    t.hour24 = bcd2dec(rawHour & 0x3F);
  }

  t.dow   = (Wire.read() & 0x07) - 1; // Convert 1-7 to 0-6 (0=Sun)
  if (t.dow < 0) t.dow = 0;
  t.day   = bcd2dec(Wire.read() & 0x3F);
  t.month = bcd2dec(Wire.read() & 0x1F);
  t.year  = 2000 + bcd2dec(Wire.read());
  t.valid = (t.year >= 2024);
  return t.valid;
}

// Write registers to DS3231 via I2C
static bool ds3231Write(int y, int m, int d, int h, int mi, int s, int dow) {
  Wire.beginTransmission(DS3231_I2C_ADDR);
  Wire.write(0x00);
  Wire.write(dec2bcd(s & 0x7F));
  Wire.write(dec2bcd(mi & 0x7F));
  Wire.write(dec2bcd(h & 0x3F)); // 24-hour format
  Wire.write(dec2bcd((dow % 7) + 1));
  Wire.write(dec2bcd(d & 0x3F));
  Wire.write(dec2bcd(m & 0x1F));
  Wire.write(dec2bcd((y >= 2000 ? y - 2000 : y) & 0xFF));
  return (Wire.endTransmission() == 0);
}

// ───────────────────── Public Clock API ─────────────────────
bool clockBegin() {
  // Check if physical DS3231 is detected on I2C address 0x68
  Wire.beginTransmission(DS3231_I2C_ADDR);
  gHasRtc = (Wire.endTransmission() == 0);

  if (gHasRtc) {
    Serial.println("[rtc] DS3231 Hardware RTC detected (0x68) OK.");
    MochiTime cur;
    if (ds3231Read(cur) && cur.valid) {
      struct tm tmRtc;
      memset(&tmRtc, 0, sizeof(tmRtc));
      tmRtc.tm_sec  = cur.second;
      tmRtc.tm_min  = cur.minute;
      tmRtc.tm_hour = cur.hour24;
      tmRtc.tm_mday = cur.day;
      tmRtc.tm_mon  = cur.month - 1;
      tmRtc.tm_year = cur.year - 1900;
      time_t tEpoch = mktime(&tmRtc);
      if (tEpoch > 1700000000) {
        struct timeval tv = { .tv_sec = tEpoch, .tv_usec = 0 };
        settimeofday(&tv, nullptr);
        Serial.printf("[rtc] Restored time from DS3231: %02d:%02d:%02d %02d/%02d/%04d\n",
                      cur.hour24, cur.minute, cur.second, cur.day, cur.month, cur.year);
      }
    }
  } else {
    Serial.println("[rtc] DS3231 not detected on I2C (0x68). Using ESP32 internal RTC.");
  }

  // Pre-configure NTP timezone (UTC+6 Bangladesh default: 6 * 3600s)
  configTime(6 * 3600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  // If internal time is still at epoch 1970, initialize from compile timestamp
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

  // 1. Primary source: ESP32 internal SNTP/POSIX time
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

  // 2. Secondary source: DS3231 hardware RTC
  if (gHasRtc && ds3231Read(t)) {
    return t;
  }

  return t;
}

void clockSetFromBuild() {
  const char *date = __DATE__;     // "Mmm dd yyyy"
  const char *timeStr = __TIME__;  // "hh:mm:ss"

  int year = atoi(date + 7);
  int day  = atoi(date + 4);
  int month = 1;
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int m = 0; m < 12; m++) {
    if (strncmp(date, months[m], 3) == 0) { month = m + 1; break; }
  }
  int h  = atoi(timeStr);
  int mi = atoi(timeStr + 3);
  int s  = atoi(timeStr + 6);

  if (gHasRtc) {
    ds3231Write(year, month, day, h, mi, s, 0);
  }

  struct tm tmBuild;
  memset(&tmBuild, 0, sizeof(tmBuild));
  tmBuild.tm_year = year - 1900;
  tmBuild.tm_mon  = month - 1;
  tmBuild.tm_mday = day;
  tmBuild.tm_hour = h;
  tmBuild.tm_min  = mi;
  tmBuild.tm_sec  = s;
  time_t tEpoch = mktime(&tmBuild);
  if (tEpoch > 0) {
    struct timeval tv = { .tv_sec = tEpoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
  }
  Serial.printf("[rtc] Set baseline time from build: %02d:%02d:%02d %02d/%02d/%04d\n",
                h, mi, s, day, month, year);
}

bool clockSyncNTP(long gmtOffsetSec) {
  configTime(gmtOffsetSec, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  struct tm tmNow;
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&tmNow, 300)) {
      if (gHasRtc) {
        ds3231Write(tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                    tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, tmNow.tm_wday);
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
