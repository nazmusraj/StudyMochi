#include "weather.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

namespace {

constexpr char WEATHER_HOST[] = "api.open-meteo.com";
WeatherNow cachedWeather;
uint32_t lastAttempt = 0;

String objectFrom(const String &body, const char *name) {
  String marker = String("\"") + name + "\":{";
  int start = body.indexOf(marker);
  return start < 0 ? String() : body.substring(start);
}

bool numberAfter(const String &text, const char *key, float &value) {
  int pos = text.indexOf(key);
  if (pos < 0) return false;
  pos += strlen(key);
  while (pos < (int)text.length()) {
    char c = text[pos];
    if (c != '"' && c != ':' && c != ' ' && c != '[') break;
    ++pos;
  }
  int end = pos;
  while (end < (int)text.length()) {
    char c = text[end];
    if (!(isdigit((unsigned char)c) || c == '-' || c == '.' || c == '+')) break;
    ++end;
  }
  if (end == pos) return false;
  value = text.substring(pos, end).toFloat();
  return true;
}

bool readResponseBody(WiFiClientSecure &client, String &body) {
  String line;
  bool inBody = false;
  int status = 0;
  uint32_t lastData = millis();

  while (millis() - lastData < 12000) {
    while (client.available()) {
      char c = (char)client.read();
      lastData = millis();
      if (inBody) {
        if (body.length() < 5000) body += c;
      } else if (c == '\n') {
        line.trim();
        if (status == 0 && line.startsWith("HTTP/")) {
          int firstSpace = line.indexOf(' ');
          status = firstSpace >= 0 ? line.substring(firstSpace + 1).toInt() : 0;
        }
        if (line.length() == 0) inBody = true;
        line = "";
      } else if (c != '\r' && line.length() < 240) {
        line += c;
      }
    }
    if (!client.connected() && !client.available()) break;
    delay(2);
  }
  return status == 200 && body.length() > 20;
}

}  // namespace

bool weatherNeedsRefresh(uint32_t now) {
  if (!cachedWeather.valid) {
    return lastAttempt == 0 || (uint32_t)(now - lastAttempt) >= 60000UL;
  }
  return
         (uint32_t)(now - cachedWeather.fetchedAt) >= WEATHER_REFRESH_MS;
}

bool weatherFetch(float latitude, float longitude, bool force) {
  if (!force && !weatherNeedsRefresh(millis())) return true;
  lastAttempt = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[weather] Wi-Fi is unavailable; keeping cached values");
    return false;
  }

  WiFiClientSecure client;
  // Certificate pinning can be added later without changing the weather API.
  client.setInsecure();
  client.setTimeout(10);
  if (!client.connect(WEATHER_HOST, 443)) {
    Serial.println("[weather] HTTPS connection failed");
    return false;
  }

  char path[640];
  snprintf(path, sizeof(path),
           "/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
           "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&timezone=auto&forecast_days=1&temperature_unit=celsius"
           "&wind_speed_unit=kmh&precipitation_unit=mm&timeformat=unixtime",
           latitude, longitude);

  client.printf("GET %s HTTP/1.1\r\n", path);
  client.printf("Host: %s\r\n", WEATHER_HOST);
  client.print("User-Agent: StudyMochi/2\r\nConnection: close\r\n\r\n");

  String body;
  bool responseOk = readResponseBody(client, body);
  client.stop();
  if (!responseOk) {
    Serial.println("[weather] Open-Meteo returned an invalid response");
    return false;
  }

  String current = objectFrom(body, "current");
  String daily = objectFrom(body, "daily");
  float temperature, apparent, humidity, isDay, code, wind;
  float maximum, minimum, rain, offset;
  bool ok = numberAfter(current, "\"temperature_2m\"", temperature) &&
            numberAfter(current, "\"relative_humidity_2m\"", humidity) &&
            numberAfter(current, "\"apparent_temperature\"", apparent) &&
            numberAfter(current, "\"is_day\"", isDay) &&
            numberAfter(current, "\"weather_code\"", code) &&
            numberAfter(current, "\"wind_speed_10m\"", wind) &&
            numberAfter(daily, "\"temperature_2m_max\"", maximum) &&
            numberAfter(daily, "\"temperature_2m_min\"", minimum) &&
            numberAfter(daily, "\"precipitation_probability_max\"", rain);
  if (!ok) {
    Serial.println("[weather] Required values were missing from the response");
    return false;
  }

  if (!numberAfter(body, "\"utc_offset_seconds\"", offset)) {
    offset = cachedWeather.utcOffsetSeconds;
  }

  WeatherNow fresh;
  fresh.temperatureC = temperature;
  fresh.apparentC = apparent;
  fresh.humidity = constrain((int)lroundf(humidity), 0, 100);
  fresh.isDay = isDay >= 0.5f;
  fresh.weatherCode = (int)lroundf(code);
  fresh.windKmh = max(0.0f, wind);
  fresh.maximumC = maximum;
  fresh.minimumC = minimum;
  fresh.rainProbability = constrain((int)lroundf(rain), 0, 100);
  fresh.utcOffsetSeconds = (int)lroundf(offset);
  fresh.valid = true;
  fresh.fetchedAt = millis();
  cachedWeather = fresh;

  Serial.printf("[weather] %.1f C, feels %.1f C, humidity %d%%, WMO %d, offset %+d\n",
                fresh.temperatureC, fresh.apparentC, fresh.humidity,
                fresh.weatherCode, fresh.utcOffsetSeconds);
  return true;
}

WeatherNow weatherGet() {
  return cachedWeather;
}

const char *weatherBangla(int code) {
  switch (code) {
    case 0: return "পরিষ্কার";
    case 1: return "প্রায় পরিষ্কার";
    case 2: return "আংশিক মেঘলা";
    case 3: return "মেঘলা";
    case 45: case 48: return "কুয়াশা";
    case 51: case 53: case 55: return "গুঁড়ি বৃষ্টি";
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

String weatherSentence(const WeatherNow &weather) {
  if (!weather.valid) return "আবহাওয়ার তথ্য এখনো পাওয়া যায়নি।";
  String sentence = "এখন তাপমাত্রা " + String(weather.temperatureC, 0) +
                    " ডিগ্রি সেলসিয়াস, " + weatherBangla(weather.weatherCode) +
                    "। অনুভূত তাপমাত্রা " + String(weather.apparentC, 0) +
                    " ডিগ্রি এবং আর্দ্রতা " + String(weather.humidity) + " শতাংশ।";
  return sentence;
}
