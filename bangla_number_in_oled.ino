// watch complete making video from here , please subscribe and support us
// you can buy components and this kit from www.esclabs.in 

// ==================================================
// EDISON SCIENCE CORNER  - ESCLABS (Translated to Bangla)
// ==================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> 
#include <U8g2_for_Adafruit_GFX.h>
#include "time.h"
#include <math.h>

// ==================================================
// 1. ASSETS & CONFIG
// ==================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SDA_PIN 21
#define SCL_PIN 22
#define TOUCH_PIN 4 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// --- WEATHER ICONS ---
const unsigned char bmp_clear[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0xc0, 0x80, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0xff, 0xff, 0x00, 0x06, 0xff, 0xff, 0x60, 0x06, 0xff, 0xff, 0x60, 0x06, 0xff, 0xff, 0x60, 0x00, 0xff, 0xff, 0x00, 0x3e, 0xff, 0xff, 0x7c, 0x3e, 0xff, 0xff, 0x7c, 0x3e, 0xff, 0xff, 0x7c, 0x00, 0xff, 0xff, 0x00, 0x06, 0xff, 0xff, 0x60, 0x06, 0xff, 0xff, 0x60, 0x06, 0xff, 0xff, 0x60, 0x00, 0xff, 0xff, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x01, 0x0f, 0xf0, 0x80, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_clouds[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0xf0, 0x03, 0xff, 0xff, 0xf8, 0x07, 0xff, 0xff, 0xfc, 0x07, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0xfe, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xfe, 0x07, 0xff, 0xff, 0xfc, 0x03, 0xff, 0xff, 0xf8, 0x00, 0xff, 0xff, 0xe0, 0x00, 0x3f, 0xff, 0x80, 0x00, 0x0f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_rain[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0xf0, 0x03, 0xff, 0xff, 0xf8, 0x07, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xfe, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xfe, 0x07, 0xff, 0xff, 0xfc, 0x03, 0xff, 0xff, 0xf8, 0x00, 0xff, 0xff, 0xe0, 0x00, 0x3f, 0xff, 0x80, 0x00, 0x0f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x0c, 0x00, 0x00, 0x60, 0x0c, 0x00, 0x00, 0xe0, 0x1c, 0x00, 0x00, 0xc0, 0x18, 0x00, 0x03, 0x80, 0x70, 0x00, 0x03, 0x80, 0x70, 0x00, 0x03, 0x00, 0x60, 0x00, 0x02, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char mini_sun[] PROGMEM = { 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x10, 0x08, 0x04, 0x20, 0x03, 0xc0, 0x27, 0xe4, 0x07, 0xe0, 0x07, 0xe0, 0x27, 0xe4, 0x03, 0xc0, 0x04, 0x20, 0x10, 0x08, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00 };
const unsigned char mini_cloud[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x1f, 0xf8, 0x3f, 0xfc, 0x3f, 0xfc, 0x7f, 0xfe, 0x3f, 0xfe, 0x1f, 0xfc, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char mini_rain[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x1f, 0xf8, 0x3f, 0xfc, 0x3f, 0xfc, 0x7f, 0xfe, 0x3f, 0xfe, 0x1f, 0xfc, 0x00, 0x00, 0x44, 0x44, 0x22, 0x22, 0x11, 0x11 };
const unsigned char bmp_tiny_drop[] PROGMEM = { 0x10, 0x38, 0x7c, 0xfe, 0xfe, 0x7c, 0x38, 0x00 };
const unsigned char bmp_heart[] PROGMEM = { 0x00, 0x00, 0x0c, 0x60, 0x1e, 0xf0, 0x3f, 0xf8, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x3f, 0xf8, 0x1f, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_zzz[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x30, 0x00, 0x7e, 0x00, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x30, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_anger[] PROGMEM = { 0x00, 0x00, 0x11, 0x10, 0x2a, 0x90, 0x44, 0x40, 0x80, 0x20, 0x80, 0x20, 0x44, 0x40, 0x2a, 0x90, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// --- GLOBALS ---
int currentPage = 0;
bool highBrightness = true;
int tapCounter = 0;
unsigned long lastTapTime = 0;
bool lastPinState = false;
unsigned long pressStartTime = 0;
bool isLongPressHandled = false;
const unsigned long LONG_PRESS_TIME = 800;
const unsigned long DOUBLE_TAP_DELAY = 300;
unsigned long lastPageSwitch = 0;
const unsigned long PAGE_INTERVAL = 8000;

#define MOOD_NORMAL 0
#define MOOD_HAPPY 1
#define MOOD_SURPRISED 2
#define MOOD_SLEEPY 3
#define MOOD_ANGRY 4
#define MOOD_SAD 5
#define MOOD_EXCITED 6
#define MOOD_LOVE 7
#define MOOD_SUSPICIOUS 8
int currentMood = MOOD_NORMAL;

String city;       
String countryCode; 
String apiKey;   
String wifiSsid; 
String wifiPass; 
unsigned long lastWeatherUpdate = 0;
float temperature = 0.0;
float feelsLike = 0.0;
int humidity = 0;
String weatherMain = "Loading";
String weatherDesc = "অপেক্ষা করুন...";

struct ForecastDay {
  String dayName;
  int temp;
  String iconType;
};
ForecastDay fcast[3];
const char* ntpServer = "pool.ntp.org";
String tzString;   

// বাংলা সংখ্যা কনভার্টার
String getBanglaNumber(int number) {
  String engNum = String(number);
  String banglaNum = "";
  for (int i = 0; i < engNum.length(); i++) {
    char c = engNum[i];
    switch (c) {
      case '0': banglaNum += "০"; break;
      case '1': banglaNum += "১"; break;
      case '2': banglaNum += "২"; break;
      case '3': banglaNum += "৩"; break;
      case '4': banglaNum += "৪"; break;
      case '5': banglaNum += "৫"; break;
      case '6': banglaNum += "৬"; break;
      case '7': banglaNum += "৭"; break;
      case '8': banglaNum += "৮"; break;
      case '9': banglaNum += "৯"; break;
      default: banglaNum += c; break;
    }
  }
  return banglaNum;
}

// বাংলা মাস ও দিনের নাম
const char* banglaDays[] = { "রবি", "সোম", "মঙ্গল", "বুধ", "বৃহঃ", "শুক্র", "শনি" };
const char* banglaMonths[] = {"জানু", "ফেব", "মার্চ", "এপ্রিল", "মে", "জুন", "জুলাই", "আগস্ট", "সেপ্টে", "অক্টো", "নভে", "ডিসে"};

// বাংলা আবহাওয়ার নাম
String getBanglaWeather(String wMain) {
  if(wMain == "Clear") return "পরিষ্কার আকাশ";
  if(wMain == "Clouds") return "মেঘলা আকাশ";
  if(wMain == "Rain") return "বৃষ্টিপাত";
  if(wMain == "Drizzle") return "গুঁড়ি বৃষ্টি";
  if(wMain == "Thunderstorm") return "বজ্রঝড়";
  if(wMain == "Snow") return "তুষারপাত";
  if(wMain == "Mist" || wMain == "Fog" || wMain == "Haze") return "কুয়াশা";
  return "সাধারণ";
}

// ==================================================
// 2. ULTRA PRO PHYSICS ENGINE 
// ==================================================
struct Eye {
  float x, y, w, h, targetX, targetY, targetW, targetH, pupilX, pupilY, targetPupilX, targetPupilY;
  float velX, velY, velW, velH, pVelX, pVelY;
  float k = 0.12, d = 0.60, pk = 0.08, pd = 0.50;  
  bool blinking;
  unsigned long lastBlink, nextBlinkTime;

  void init(float _x, float _y, float _w, float _h) {
    x = targetX = _x; y = targetY = _y; w = targetW = _w; h = targetH = _h;
    pupilX = targetPupilX = 0; pupilY = targetPupilY = 0;
    nextBlinkTime = millis() + random(1000, 4000);
  }

  void update() {
    float ax = (targetX - x) * k; float ay = (targetY - y) * k;
    float aw = (targetW - w) * k; float ah = (targetH - h) * k;
    velX = (velX + ax) * d; velY = (velY + ay) * d;
    velW = (velW + aw) * d; velH = (velH + ah) * d;
    x += velX; y += velY; w += velW; h += velH;
    float pax = (targetPupilX - pupilX) * pk;
    float pay = (targetPupilY - pupilY) * pk;
    pVelX = (pVelX + pax) * pd; pVelY = (pVelY + pay) * pd;
    pupilX += pVelX; pupilY += pVelY;
  }
};

Eye leftEye, rightEye;
unsigned long lastSaccade = 0, saccadeInterval = 3000;
float breathVal = 0;

// ----- ARDUINO IDE BUG FIX (Prototyping) -----
void drawUltraProEye(Eye& e, bool isLeft);
void drawEyelidMask(float x, float y, float w, float h, int mood, bool isLeft);
// ----------------------------------------------


// ==================================================
// 2b. CONFIG PORTAL 
// ==================================================
#define CONFIG_AP_SSID   "DeskBuddy-Setup"
#define CONFIG_AP_PASS   "12345678"
#define CONFIG_HOLD_MS   3000

Preferences prefs;
WebServer configServer(80);
bool inConfigMode = false;

void loadConfig() {
  prefs.begin("deskbuddy", true);
  wifiSsid    = prefs.getString("ssid", "");
  wifiPass    = prefs.getString("pass", "");
  apiKey      = prefs.getString("apikey", "");
  city        = prefs.getString("city", "");
  countryCode = prefs.getString("country", "");
  tzString    = prefs.getString("tz", "");
  prefs.end();
  
  if (wifiSsid.isEmpty()) {
    wifiSsid    = "Prof Mashud";
    wifiPass    = "Mashud4321";
    apiKey      = "45fcf5807a5920e2006c2b8a077d423f";
    city        = "Dhaka";
    countryCode = "BD";
    tzString    = "<+06>-6"; // বাংলাদেশের ডিফল্ট টাইমজোন
  }
}

void saveConfig(const String& s, const String& p, const String& ak,
                const String& cty, const String& ctry, const String& tz) {
  prefs.begin("deskbuddy", false);
  prefs.putString("ssid", s); prefs.putString("pass", p);
  prefs.putString("apikey", ak); prefs.putString("city", cty);
  prefs.putString("country", ctry); prefs.putString("tz", tz);
  prefs.end();
}

void handleConfigRoot() {
  prefs.begin("deskbuddy", true);
  String sSsid = prefs.getString("ssid", "");
  String sApik = prefs.getString("apikey", "45fcf5807a5920e2006c2b8a077d423f");
  String sCity = prefs.getString("city", "Dhaka");
  String sCtry = prefs.getString("country", "BD");
  String sTz   = prefs.getString("tz", "<+06>-6");
  prefs.end();

  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>স্মার্ট ডেস্ক গ্যাজেট সেটআপ</title>
<meta charset="UTF-8">
<style>
body{font-family:sans-serif;max-width:420px;margin:30px auto;padding:24px;background:#0c1929;color:#e8f4fc;}
h1{color:#5ba3f5;margin-bottom:8px;}
input{width:100%;padding:10px;margin:6px 0;border:1px solid #2d4a6f;border-radius:6px;box-sizing:border-box;background:#1a2d47;color:#e8f4fc;}
input:focus{outline:none;border-color:#5ba3f5;}
button{width:100%;padding:12px;background:#3498db;color:#fff;border:none;border-radius:6px;font-size:16px;cursor:pointer;margin-top:16px;}
button:hover{background:#2980b9;}
label{display:block;margin-top:14px;color:#8ab4e8;font-size:14px;}
.section{margin-top:20px;padding-top:16px;border-top:1px solid #1e3a5f;}
.section-title{color:#5ba3f5;font-size:13px;margin-bottom:8px;}
</style></head><body>
<h1>ডেস্ক গ্যাজেট সেটআপ</h1>
<form action="/save" method="POST">
<label>ওয়াইফাইয়ের নাম (SSID)</label><input name="ssid" placeholder="ওয়াইফাইয়ের নাম" value=")rawliteral";
  html += sSsid;
  html += R"rawliteral(">
<label>ওয়াইফাইয়ের পাসওয়ার্ড</label><input name="pass" type="password" placeholder="পাসওয়ার্ড">
<div class="section"><div class="section-title">আবহাওয়া সেটিংস (OpenWeatherMap)</div>
<label>এপিআই কি (API Key)</label><input name="apikey" placeholder="API key" value=")rawliteral";
  html += sApik;
  html += R"rawliteral(">
<label>শহরের নাম (ইংরেজিতে লিখুন)</label><input name="city" placeholder="যেমন: Dhaka" value=")rawliteral";
  html += sCity;
  html += R"rawliteral(">
<label>দেশের কোড (ইংরেজিতে)</label><input name="country" placeholder="যেমন: BD" value=")rawliteral";
  html += sCtry;
  html += R"rawliteral(">
</div>
<div class="section"><div class="section-title">সময় সেটিংস</div>
<label>টাইমজোন (বাংলাদেশের জন্য <+06>-6)</label><input name="tz" placeholder="যেমন: <+06>-6" value=")rawliteral";
  html += sTz;
  html += R"rawliteral(">
</div>
<button type="submit">সেভ করুন এবং রিস্টার্ট দিন</button>
</form></body></html>)rawliteral";
  configServer.send(200, "text/html", html);
}

void handleConfigSave() {
  if (!configServer.hasArg("ssid") || configServer.arg("ssid").length() == 0) {
    configServer.send(400, "text/plain", "SSID required");
    return;
  }
  saveConfig(configServer.arg("ssid"), configServer.arg("pass"), configServer.arg("apikey"),
             configServer.arg("city"), configServer.arg("country"), configServer.arg("tz"));
  configServer.send(200, "text/html",
    "<html><meta charset='UTF-8'><body style='font-family:sans-serif;background:#0c1929;color:#e8f4fc;padding:40px;'>"
    "<h2 style='color:#5ba3f5'>সেভ হয়েছে!</h2><p>গ্যাজেটটি রিস্টার্ট হচ্ছে...</p></body></html>");
  delay(2000);
  ESP.restart();
}

void startConfigPortal() {
  inConfigMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASS);
  configServer.on("/", handleConfigRoot);
  configServer.on("/save", HTTP_POST, handleConfigSave);
  configServer.begin();
  
  display.clearDisplay();
  u8g2Fonts.setCursor(0, 16);
  u8g2Fonts.print("সেটআপ মোড চালু!");
  u8g2Fonts.setCursor(0, 32);
  u8g2Fonts.print("কানেক্ট করুন:");
  u8g2Fonts.setCursor(0, 48);
  u8g2Fonts.print(CONFIG_AP_SSID);
  display.display();
}

// ==================================================
// 3. LOGIC & NETWORK
// ==================================================
const unsigned char* getBigIcon(String w) {
  if (w == "Clear") return bmp_clear;
  if (w == "Clouds") return bmp_clouds;
  if (w == "Rain" || w == "Drizzle") return bmp_rain;
  return bmp_clouds;
}
const unsigned char* getMiniIcon(String w) {
  if (w == "Clear") return mini_sun;
  if (w == "Rain" || w == "Drizzle" || w == "Thunderstorm") return mini_rain;
  return mini_cloud;
}

void updateMoodBasedOnWeather() {
  int m = MOOD_NORMAL;
  if (weatherMain == "Clear") m = MOOD_HAPPY;
  else if (weatherMain == "Rain" || weatherMain == "Drizzle") m = MOOD_SAD;
  else if (weatherMain == "Thunderstorm") m = MOOD_SURPRISED;
  else if (weatherMain == "Clouds") m = MOOD_NORMAL;
  else if (temperature > 25) m = MOOD_EXCITED;
  else if (temperature < 5) m = MOOD_SLEEPY;
  currentMood = m;
}

void handleTouch() {
  bool currentPinState = digitalRead(TOUCH_PIN);
  unsigned long now = millis();
  if (currentPinState && !lastPinState) {
    pressStartTime = now;
    isLongPressHandled = false;
  } else if (currentPinState && lastPinState) {
    if ((now - pressStartTime > LONG_PRESS_TIME) && !isLongPressHandled) {
      lastPageSwitch = now;
      if (currentPage == 0) {
        currentMood++;
        if (currentMood > MOOD_SUSPICIOUS) currentMood = 0;
        lastSaccade = 0;  
      } else if (currentPage == 1) currentPage = 3;
      else if (currentPage == 2) currentPage = 4;
      isLongPressHandled = true;
    }
  } else if (!currentPinState && lastPinState) {
    if ((now - pressStartTime < LONG_PRESS_TIME) && !isLongPressHandled) {
      tapCounter++;
      lastTapTime = now;
    }
  }
  lastPinState = currentPinState;
  if (tapCounter > 0) {
    if (now - lastTapTime > DOUBLE_TAP_DELAY) {
      lastPageSwitch = now;
      if (tapCounter == 2) {
        highBrightness = !highBrightness;
        display.dim(!highBrightness);
      } else if (tapCounter == 1) {
        if (currentPage == 3) currentPage = 1;
        else if (currentPage == 4) currentPage = 2;
        else {
          currentPage++;
          if (currentPage > 2) currentPage = 0;
        }
      }
      tapCounter = 0;
    }
  }
}

void getWeatherAndForecast() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
  http.begin(url);
  if (http.GET() == 200) {
    JSONVar myObject = JSON.parse(http.getString());
    if (JSON.typeof(myObject) != "undefined") {
      temperature = double(myObject["main"]["temp"]);
      feelsLike = double(myObject["main"]["feels_like"]);
      humidity = int(myObject["main"]["humidity"]);
      weatherMain = (const char*)myObject["weather"][0]["main"];
      updateMoodBasedOnWeather();
    }
  }
  http.end();
  
  url = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
  http.begin(url);
  if (http.GET() == 200) {
    JSONVar fo = JSON.parse(http.getString());
    if (JSON.typeof(fo) != "undefined") {
      struct tm t; getLocalTime(&t);
      int indices[3] = { 7, 15, 23 };
      for (int i = 0; i < 3; i++) {
        fcast[i].temp = (int)double(fo["list"][indices[i]]["main"]["temp"]);
        fcast[i].iconType = (const char*)fo["list"][indices[i]]["weather"][0]["main"];
        fcast[i].dayName = banglaDays[(t.tm_wday + i + 1) % 7];
      }
    }
  }
  http.end();
}

// ==================================================
// 4. DRAWING & ANIMATION
// ==================================================

void drawEyelidMask(float x, float y, float w, float h, int mood, bool isLeft) {
  int ix = x, iy = y, iw = w, ih = h;
  display.setTextColor(BLACK);
  if (mood == MOOD_ANGRY) {
    if (isLeft) for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, BLACK);
    else for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, BLACK);
  }
  if (mood == MOOD_SAD) {
    if (isLeft) for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, BLACK);
    else for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, BLACK);
  }
  if (mood == MOOD_HAPPY || mood == MOOD_LOVE || mood == MOOD_EXCITED) {
    display.fillRect(ix, iy + ih - 12, iw, 14, BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 6, iw / 1.3, BLACK);  
  }
  if (mood == MOOD_SLEEPY) display.fillRect(ix, iy, iw, ih / 2 + 2, BLACK);
  if (mood == MOOD_SUSPICIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, ih / 2 - 2, BLACK);
    else display.fillRect(ix, iy + ih - 8, iw, 8, BLACK);
  }
}

void drawUltraProEye(Eye& e, bool isLeft) {
  int ix = e.x, iy = e.y, iw = e.w, ih = e.h;
  int r = (iw < 20) ? 3 : 8;
  display.fillRoundRect(ix, iy, iw, ih, r, WHITE);
  
  int px = (ix + iw / 2) + e.pupilX - (iw / 4.4);
  int py = (iy + ih / 2) + e.pupilY - (ih / 4.4);
  if (px < ix) px = ix; if (px + iw/2.2 > ix + iw) px = ix + iw - iw/2.2;
  if (py < iy) py = iy; if (py + ih/2.2 > iy + ih) py = iy + ih - ih/2.2;
  
  display.fillRoundRect(px, py, iw/2.2, ih/2.2, r / 2, BLACK);
  if (iw > 15 && ih > 15) display.fillCircle(px + iw/2.2 - 4, py + 4, 2, WHITE);
  drawEyelidMask(e.x, e.y, e.w, e.h, currentMood, isLeft);
}

void updatePhysicsAndMood() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0) * 1.5;  
  if (now > leftEye.nextBlinkTime) {
    leftEye.blinking = rightEye.blinking = true;
    leftEye.lastBlink = now;
    leftEye.nextBlinkTime = now + random(2000, 6000);
  }
  if (leftEye.blinking) {
    leftEye.targetH = rightEye.targetH = 2;  
    if (now - leftEye.lastBlink > 120) leftEye.blinking = rightEye.blinking = false;
  }
  if (!leftEye.blinking && now - lastSaccade > saccadeInterval) {
    lastSaccade = now; saccadeInterval = random(500, 3000);
    int dir = random(0, 10);
    float lx = 0, ly = 0;
    if (dir == 4) { lx = -6; ly = -4; } else if (dir == 5) { lx = 6; ly = -4; }  
    else if (dir == 6) { lx = -6; ly = 4; } else if (dir == 7) { lx = 6; ly = 4; }  
    else if (dir == 8) { lx = 8; ly = 0; } else if (dir == 9) { lx = -8; ly = 0; }  
    leftEye.targetPupilX = rightEye.targetPupilX = lx;
    leftEye.targetPupilY = rightEye.targetPupilY = ly;
    leftEye.targetX = 18 + (lx * 0.3); leftEye.targetY = 14 + (ly * 0.3);
    rightEye.targetX = 74 + (lx * 0.3); rightEye.targetY = 14 + (ly * 0.3);
  }
  if (!leftEye.blinking) {
    float baseH = 36 + breathVal;
    leftEye.targetW = rightEye.targetW = 36;
    leftEye.targetH = rightEye.targetH = baseH;
    if (currentMood == MOOD_HAPPY || currentMood == MOOD_LOVE) { leftEye.targetW=rightEye.targetW=40; leftEye.targetH=rightEye.targetH=32; }
    if (currentMood == MOOD_SURPRISED) { leftEye.targetW=rightEye.targetW=30; leftEye.targetH=rightEye.targetH=45; }
    if (currentMood == MOOD_SLEEPY) { leftEye.targetW=rightEye.targetW=38; leftEye.targetH=rightEye.targetH=30; }
    if (currentMood == MOOD_ANGRY) { leftEye.targetW=rightEye.targetW=34; leftEye.targetH=rightEye.targetH=32; }
    if (currentMood == MOOD_SAD) { leftEye.targetW=rightEye.targetW=34; leftEye.targetH=rightEye.targetH=40; }
    if (currentMood == MOOD_SUSPICIOUS) { leftEye.targetH=20; rightEye.targetH=42; }
  }
  leftEye.update(); rightEye.update();
}

void drawEmoPage() {
  updatePhysicsAndMood();
  if (currentMood == MOOD_LOVE) display.drawBitmap(56, 0, bmp_heart, 16, 16, WHITE);
  else if (currentMood == MOOD_SLEEPY) display.drawBitmap(110, 0, bmp_zzz, 16, 16, WHITE);
  else if (currentMood == MOOD_ANGRY) display.drawBitmap(56, 0, bmp_anger, 16, 16, WHITE);
  drawUltraProEye(leftEye, true); drawUltraProEye(rightEye, false);
}

// --- STANDARD PAGES ---
void drawForecastPage() {
  display.fillRect(0, 0, 128, 16, WHITE);
  u8g2Fonts.setForegroundColor(BLACK);
  
  String title = "৩-দিনের পূর্বাভাস";
  int w = u8g2Fonts.getUTF8Width(title.c_str());
  u8g2Fonts.setCursor((128-w)/2, 13);
  u8g2Fonts.print(title);
  
  u8g2Fonts.setForegroundColor(WHITE);
  display.drawLine(42, 16, 42, 64, WHITE);
  display.drawLine(85, 16, 85, 64, WHITE);
  
  for (int i = 0; i < 3; i++) {
    int centerX = (i * 43) + 21;
    String d = fcast[i].dayName;
    w = u8g2Fonts.getUTF8Width(d.c_str());
    u8g2Fonts.setCursor(centerX - (w/2), 28);
    u8g2Fonts.print(d);
    
    display.drawBitmap(centerX - 8, 30, getMiniIcon(fcast[i].iconType), 16, 16, WHITE);
    
    String tempStr = getBanglaNumber(fcast[i].temp) + "°";
    w = u8g2Fonts.getUTF8Width(tempStr.c_str());
    u8g2Fonts.setCursor(centerX - (w/2), 60);
    u8g2Fonts.print(tempStr);
  }
}

void drawClock() {
  struct tm t;
  if (!getLocalTime(&t)) {
    u8g2Fonts.setCursor(20, 35);
    u8g2Fonts.print("সময় মেলাচ্ছে...");
    return;
  }
  
  String ampm = (t.tm_hour >= 12) ? "পিএম" : "এএম";
  int h12 = t.tm_hour % 12; if (h12 == 0) h12 = 12;
  
  String timeStr = getBanglaNumber(h12) + ":";
  if(t.tm_min < 10) timeStr += "০";
  timeStr += getBanglaNumber(t.tm_min);
  
  String dateStr = String(banglaDays[t.tm_wday]) + ", " + getBanglaNumber(t.tm_mday) + " " + banglaMonths[t.tm_mon];

  int w1 = u8g2Fonts.getUTF8Width(ampm.c_str());
  u8g2Fonts.setCursor(128 - w1 - 2, 14);
  u8g2Fonts.print(ampm);
  
  int w2 = u8g2Fonts.getUTF8Width(timeStr.c_str());
  u8g2Fonts.setCursor((128 - w2) / 2, 40);
  u8g2Fonts.print(timeStr);
  
  int w3 = u8g2Fonts.getUTF8Width(dateStr.c_str());
  u8g2Fonts.setCursor((128 - w3) / 2, 60);
  u8g2Fonts.print(dateStr);
}

void drawWeatherCard() {
  if (WiFi.status() != WL_CONNECTED) {
    u8g2Fonts.setCursor(25, 35); u8g2Fonts.print("ওয়াইফাই নেই"); return;
  }
  display.drawBitmap(96, 0, getBigIcon(weatherMain), 32, 32, WHITE);
  
  u8g2Fonts.setCursor(0, 14);
  String c = city; 
  if (c.length() > 10) c = c.substring(0, 9) + ".";
  u8g2Fonts.print(c);
  
  String tempStr = getBanglaNumber((int)temperature) + "°C";
  u8g2Fonts.setCursor(0, 48);
  u8g2Fonts.print(tempStr);
  
  display.drawBitmap(88, 32, bmp_tiny_drop, 8, 8, WHITE);
  u8g2Fonts.setCursor(100, 40);
  u8g2Fonts.print(getBanglaNumber(humidity) + "%");
  
  u8g2Fonts.setCursor(88, 52);
  u8g2Fonts.print("~" + getBanglaNumber((int)feelsLike) + "°C");
  
  display.drawLine(0, 52, 128, 52, WHITE);
  
  u8g2Fonts.setCursor(0, 64);
  u8g2Fonts.print(getBanglaWeather(weatherMain));
}

void drawWorldClock() {
  time_t now; time(&now);
  time_t bdEpoch = now + (6 * 3600);       
  time_t makkahEpoch = now + (3 * 3600);   
  
  struct tm* bdtm = gmtime(&bdEpoch);
  int b_h = bdtm->tm_hour; int b_m = bdtm->tm_min;
  struct tm* mktm = gmtime(&makkahEpoch);
  int m_h = mktm->tm_hour; int m_m = mktm->tm_min;
  
  display.fillRect(0, 0, 128, 16, WHITE);
  u8g2Fonts.setForegroundColor(BLACK);
  
  String title = "বিশ্ব ঘড়ি";
  int w = u8g2Fonts.getUTF8Width(title.c_str());
  u8g2Fonts.setCursor((128-w)/2, 13);
  u8g2Fonts.print(title);
  
  u8g2Fonts.setForegroundColor(WHITE);
  display.drawLine(64, 18, 64, 54, WHITE);
  
  u8g2Fonts.setCursor(4, 30); u8g2Fonts.print("বাংলাদেশ");
  String bStr = getBanglaNumber(b_h) + ":";
  if(b_m < 10) bStr += "০"; bStr += getBanglaNumber(b_m);
  u8g2Fonts.setCursor(12, 48); u8g2Fonts.print(bStr);
  
  u8g2Fonts.setCursor(76, 30); u8g2Fonts.print("মক্কা");
  String mStr = getBanglaNumber(m_h) + ":";
  if(m_m < 10) mStr += "০"; mStr += getBanglaNumber(m_m);
  u8g2Fonts.setCursor(82, 48); u8g2Fonts.print(mStr);
  
  String exitTxt = "বের হতে টাচ করুন";
  int wExit = u8g2Fonts.getUTF8Width(exitTxt.c_str());
  u8g2Fonts.setCursor((128 - wExit) / 2, 64);
  u8g2Fonts.print(exitTxt);
}

// ==================================================
// 5. BOOT & MAIN
// ==================================================
void playBootAnimation() {
  int cx = 64, cy = 32;
  for (int r = 0; r < 80; r += 4) { display.clearDisplay(); display.fillCircle(cx, cy, r, WHITE); display.display(); delay(10); }
  for (int r = 0; r < 80; r += 4) { display.clearDisplay(); display.fillCircle(cx, cy, 80, WHITE); display.fillCircle(cx, cy, r, BLACK); display.display(); delay(10); }

  String bootText = "ইএসসি ল্যাবস";
  int w = u8g2Fonts.getUTF8Width(bootText.c_str());
  display.clearDisplay();
  u8g2Fonts.setCursor((128 - w) / 2, 38);
  u8g2Fonts.print(bootText);
  display.display();
  delay(2000);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  
  u8g2Fonts.begin(display);
  u8g2Fonts.setFont(u8g2_font_unifont_t_bengali); 
  u8g2Fonts.setFontMode(1);                 
  u8g2Fonts.setFontDirection(0);            
  u8g2Fonts.setForegroundColor(WHITE);  

  bool forceConfig = false;
  for (unsigned long t = millis(); millis() - t < CONFIG_HOLD_MS; ) {
    if (digitalRead(TOUCH_PIN)) { forceConfig = true; break; }
    delay(80);
  }

  loadConfig();
  if (forceConfig) { startConfigPortal(); return; }

  leftEye.init(18, 14, 36, 36); rightEye.init(74, 14, 36, 36);

  playBootAnimation();

  display.clearDisplay();
  u8g2Fonts.setCursor(20, 36);
  u8g2Fonts.print("কানেক্ট হচ্ছে...");
  display.display();
  
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart < 15000)) delay(200);
  if (WiFi.status() != WL_CONNECTED) { startConfigPortal(); return; }
  
  configTime(0, 0, ntpServer);
  setenv("TZ", tzString.c_str(), 1); tzset();
  getWeatherAndForecast();
  lastWeatherUpdate = millis(); lastPageSwitch = millis();
}

void loop() {
  if (inConfigMode) { configServer.handleClient(); return; }
  
  unsigned long now = millis();
  handleTouch();
  if (now - lastWeatherUpdate > 600000) { getWeatherAndForecast(); lastWeatherUpdate = now; }

  if (currentPage < 3 && now - lastPageSwitch > PAGE_INTERVAL) {
    currentPage++; if (currentPage > 2) currentPage = 0;
    lastPageSwitch = now; lastSaccade = 0;
  }

  display.clearDisplay();
  switch (currentPage) {
    case 0: drawEmoPage(); break;
    case 1: drawClock(); break;
    case 2: drawWeatherCard(); break;
    case 3: drawWorldClock(); break;
    case 4: drawForecastPage(); break;
  }
  display.display();
}