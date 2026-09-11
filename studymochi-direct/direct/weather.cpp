#include "weather.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "rtcclock.h"          // banglaDigits()

static WeatherNow gW;

#define WX_HOST  "api.open-meteo.com"
#define WX_FRESH_MS (15UL * 60UL * 1000UL)      // ১৫ মিনিট

// ⚠️ open-meteo একই নামগুলো **দুবার** পাঠায়:
//     "current_units":{"temperature_2m":"°C", ...}      ← একক, লেখা
//     "current":{"temperature_2m":31.4, ...}            ← আসল সংখ্যা
// প্রথমে যেটা পাই সেটা নিলে "°C"-তে গিয়ে ঠেকি আর কিছুই পাই না।
// তাই আগে "current":{ খুঁজে নিই, তারপর সেখান থেকে পড়ি।
// (আসল উত্তর দিয়ে টেস্ট করেই এটা ধরা পড়েছিল।)
static String currentBlock(const String &body) {
  int i = body.indexOf("\"current\":{");
  return i < 0 ? body : body.substring(i);
}

// JSON থেকে একটা সংখ্যা তুলে আনি। উত্তরটা ছোট আর গঠন সরল,
// তাই পুরো JSON পার্সার (ArduinoJson) টানার দরকার নেই।
static bool numAfter(const String &s, const char *key, float &out) {
  int i = s.indexOf(key);
  if (i < 0) return false;
  i += strlen(key);
  while (i < (int)s.length() && (s[i] == '"' || s[i] == ':' || s[i] == ' ')) i++;
  int j = i;
  while (j < (int)s.length() &&
         (isdigit((unsigned char)s[j]) || s[j] == '-' || s[j] == '.')) j++;
  if (j == i) return false;
  out = s.substring(i, j).toFloat();
  return true;
}

bool weatherFetch(float lat, float lon, bool force) {
  if (!force && gW.valid && (millis() - gW.fetchedAt) < WX_FRESH_MS)
    return true;                                  // এখনো টাটকা
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[wx] WiFi nei");
    return false;
  }

  WiFiClientSecure c;
  c.setInsecure();                                // ঘরোয়া ব্যবহারে যথেষ্ট
  c.setTimeout(10);
  if (!c.connect(WX_HOST, 443)) {
    Serial.println("[wx] connect holo na");
    return false;
  }

  char path[220];
  snprintf(path, sizeof(path),
           "/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m",
           lat, lon);

  c.printf("GET %s HTTP/1.1\r\n", path);
  c.printf("Host: %s\r\n", WX_HOST);
  c.print("Connection: close\r\n\r\n");

  // হেডার পেরিয়ে যাই
  uint32_t t0 = millis();
  String line, body;
  bool inBody = false;
  while (millis() - t0 < 12000) {
    while (c.available()) {
      char ch = (char)c.read();
      t0 = millis();
      if (inBody) {
        if (body.length() < 1500) body += ch;
        continue;
      }
      if (ch == '\n') {
        line.trim();
        if (line.length() == 0) { inBody = true; }
        line = "";
      } else if (ch != '\r') {
        if (line.length() < 200) line += ch;
      }
    }
    if (!c.connected() && !c.available()) break;
    delay(5);
  }
  c.stop();

  if (body.length() < 20) {
    Serial.println("[wx] khali uttor");
    return false;
  }

  String cur = currentBlock(body);
  float t, h, code, wind;
  bool ok = numAfter(cur, "\"temperature_2m\"", t)
         && numAfter(cur, "\"weather_code\"", code);
  if (!ok) {
    Serial.print("[wx] bujhte parlam na: ");
    Serial.println(body.substring(0, 160));
    return false;
  }
  gW.tempC = t;
  gW.code  = (int)code;
  gW.humidity = numAfter(cur, "\"relative_humidity_2m\"", h) ? (int)h : 0;
  gW.windKmh  = numAfter(cur, "\"wind_speed_10m\"", wind) ? wind : 0;
  gW.valid = true;
  gW.fetchedAt = millis();
  Serial.printf("[wx] %.1f C, %d%%, code %d\n", gW.tempC, gW.humidity, gW.code);
  return true;
}

WeatherNow weatherGet() { return gW; }

// WMO আবহাওয়া কোড — https://open-meteo.com/en/docs
const char *weatherBangla(int code) {
  switch (code) {
    case 0:  return "পরিষ্কার আকাশ";
    case 1:  return "প্রায় পরিষ্কার";
    case 2:  return "আংশিক মেঘলা";
    case 3:  return "মেঘলা";
    case 45: case 48: return "কুয়াশা";
    case 51: case 53: case 55: return "ঝিরঝিরে বৃষ্টি";
    case 56: case 57: return "ঠান্ডা গুঁড়ি বৃষ্টি";
    case 61: return "হালকা বৃষ্টি";
    case 63: return "বৃষ্টি";
    case 65: return "ভারী বৃষ্টি";
    case 66: case 67: return "বরফ-বৃষ্টি";
    case 71: case 73: case 75: case 77: return "তুষারপাত";
    case 80: return "হালকা বর্ষণ";
    case 81: return "বর্ষণ";
    case 82: return "প্রবল বর্ষণ";
    case 85: case 86: return "তুষার বর্ষণ";
    case 95: return "বজ্রসহ বৃষ্টি";
    case 96: case 99: return "শিলাবৃষ্টি";
    default: return "জানা নেই";
  }
}

String weatherSentence(const WeatherNow &w) {
  if (!w.valid) return "Abohawa-r khobor ekhono ani ni.";
  // মোচি যেহেতু বাংলা বলে, তাকে রোমান হরফে নির্দেশ দিই —
  // Live API রোমান বাংলা ভালোই বোঝে আর বাংলায় বলে।
  char s[220];
  snprintf(s, sizeof(s),
           "Ekhon baire %.0f degree, %s. Battash %.0f km/h, "
           "battasher olo %d%%. Ei niye ek line-e amake bolo.",
           w.tempC, weatherBangla(w.code), w.windKmh, w.humidity);
  return String(s);
}
