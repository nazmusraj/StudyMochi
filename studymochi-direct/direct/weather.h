// ════════════════════════════════════════════════════════════════
//   আবহাওয়া — Open-Meteo থেকে।
//
//   কেন Open-Meteo: **API key লাগে না**, ফ্রি, আর ব্যক্তিগত
//   ব্যবহারে কোনো সীমা নেই। OpenWeatherMap-এর মতো আলাদা অ্যাকাউন্ট
//   খুলে key জোগাড় করার ঝামেলা নেই — একটা HTTPS GET-ই যথেষ্ট।
//
//   ডিফল্ট জায়গা ঢাকা (২৩.৮১, ৯০.৪১)। পোর্টাল থেকে বদলানো যায়।
//
//   বাড়তি কোনো লাইব্রেরি লাগে না — WiFiClientSecure দিয়েই GET,
//   আর উত্তরটা ছোট বলে হাতে লেখা পার্সারই যথেষ্ট (ArduinoJson
//   ইনস্টল করার দরকার নেই)।
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

// নেট থেকে টেনে আনে। ১৫ মিনিটের মধ্যে আগে আনা থাকলে সেটাই দেয়
// (force = true দিলে জোর করে আবার আনে)।
bool weatherFetch(float lat, float lon, bool force = false);

WeatherNow weatherGet();

// WMO কোডকে বাংলা কথায় বদলায় — "ঝিরঝিরে বৃষ্টি" ইত্যাদি
const char *weatherBangla(int code);

// মোচি মুখে যা বলবে, সেই বাক্যটা বানায়
String weatherSentence(const WeatherNow &w);
