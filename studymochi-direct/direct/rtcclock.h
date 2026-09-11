// ════════════════════════════════════════════════════════════════
//   DS3231 ঘড়ি — সময় জমা থাকে, কারেন্ট গেলেও হারায় না।
//
//   ওয়্যারিং — OLED-এর সাথে **একই I2C বাসে**, বাড়তি পিন লাগে না:
//       VCC → 3V3      GND → GND      SDA → GPIO 21     SCL → GPIO 22
//   (DS3231-এর ঠিকানা 0x68, OLED-এর 0x3C — সংঘর্ষ নেই)
//
//   সময় বসানো হয় দু'ভাবে:
//     ১) WiFi থাকলে **NTP** থেকে — সবচেয়ে নির্ভুল, নিজে নিজে
//     ২) NTP না পেলে কম্পাইলের সময়টা (আপনার আগের কোড যা করত)
//
//   RTClib লাইব্রেরি লাগবে (Adafruit) — আপনার RTC_clock.ino-তে
//   এটাই ব্যবহার করেছেন, তাই ইনস্টল করাই আছে।
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

struct MochiTime {
  int  hour24 = 0, minute = 0, second = 0;
  int  day = 1, month = 1, year = 2026;
  int  dow = 0;              // ০ = রবিবার
  bool valid = false;
};

// RTC খুঁজে চালু করে। না পেলে false — কোড তবু চলবে,
// শুধু ঘড়ির পর্দাটা "RTC নেই" দেখাবে।
bool clockBegin();
bool clockOk();

MochiTime clockNow();

// WiFi জুড়ে যাওয়ার পর একবার ডাকুন। NTP থেকে সময় এনে DS3231-এ
// বসায় (২ সেকেন্ডের বেশি ফারাক হলেই)। সফল হলে true।
bool clockSyncNTP(long gmtOffsetSec = 6 * 3600);   // বাংলাদেশ = UTC+6

// কম্পাইলের সময়টা বসায় — NTP না পেলে শেষ ভরসা
void clockSetFromBuild();

// "১১:৩৪" ধরনের বাংলা লেখা বানায় (UTF-8)
String banglaDigits(int n, int pad = 0);
const char *banglaDayName(int dow);       // "শুক্রবার"
const char *banglaMonthName(int m);       // "সেপ্টেম্বর"
