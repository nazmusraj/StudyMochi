// ════════════════════════════════════════════════════════════════
//   মোচির মুখ — SSD1306 OLED, কথার সাথে ঠোঁট নড়ে।
//
//   কেন Adafruit_SSD1306 ব্যবহার করলাম না
//   ─────────────────────────────────────
//   Adafruit-এর display() প্রতিবার পুরো ১০২৪ বাইট I2C-তে ঠেলে দেয়।
//   400 kHz-এ সেটা ≈ ২৩ ms। ঠোঁট নড়াতে সেকেন্ডে ২০ বার ডাকলে
//   ৪৬০ ms — মানে সেকেন্ডের প্রায় অর্ধেক সময় ESP32 শুধু ছবি পাঠাচ্ছে।
//   ততক্ষণ I2S-এর DMA খালি পড়ে থাকে আর **কথা কেটে যায়**।
//
//   তাই নিজেরাই ছোট্ট একটা ড্রাইভার। এতে SSD1306-র উইন্ডো কমান্ড
//   ব্যবহার করে **শুধু ঠোঁটের জায়গাটুকু** পাঠানো যায় — ৪১ কলাম ×
//   ২ পেজ = ৮২ বাইট ≈ ২ ms। ২০ fps-এও মাত্র ৪০ ms/সেকেন্ড।
//   বাড়তি লাভ: কোনো লাইব্রেরি ইনস্টল করতে হয় না।
//
//   RAM: ১০২৪ বাইট ফ্রেমবাফার। ফ্ল্যাশ: ~৪ KB (৫×৭ ফন্ট সহ)।
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

// মোচি এখন কী করছে
enum FaceState {
  FACE_BOOT,        // চালু হচ্ছে
  FACE_PORTAL,      // সেটআপ হটস্পট খোলা
  FACE_IDLE,        // বাটনের অপেক্ষায়
  FACE_LISTENING,   // শুনছে
  FACE_THINKING,    // গুগল ভাবছে
  FACE_SPEAKING,    // উত্তর বলছে
  FACE_WAITING,     // কোটা/নেট — অপেক্ষা করছে
  FACE_ERROR
};

// OLED না লাগানো থাকলেও নিরাপদ — begin() false দেবে, বাকি সব চুপচাপ
// কিছু না করে ফিরে যাবে। কোড কোথাও আটকাবে না।
bool faceBegin(int sda, int scl, uint8_t addr = 0x3C);
bool faceOk();

// SH1106 (১.৩") না SSD1306 (০.৯৬") — ভুল হলে পর্দায় আবর্জনা থাকে।
// faceBegin()-এর আগে ডাকুন। ডিফল্ট SH1106।
void faceSetPanel(bool sh1106);
bool faceIsSH1106();

void faceSetState(FaceState s);
FaceState faceGetState();

// ───────────────────── পর্দা ─────────────────────
// টাচ-২ একবার ছুঁলে পর্দা বদলায়। মোচি যখন শুনছে/ভাবছে/বলছে
// তখন মুখটাই দেখায় — পর্দার বাছাই তখন অপেক্ষা করে।
enum FaceScreen { SCR_FACE, SCR_CLOCK, SCR_WEATHER, SCR_POMO, SCR_TIMER, SCR_COUNT };

void       faceSetScreen(FaceScreen s);
FaceScreen faceScreen();
void       faceNextScreen();
void       faceRedraw();

// ── পর্দাগুলোর জন্য তথ্য ──
void faceClockData(int h24, int mi, int se, int day, int mon, int year,
                   int dow, bool rtcOk);
// আবহাওয়া — কথাটা WMO কোড হিসেবে দিন, ছবি face.cpp নিজে বাছবে
void faceWeatherData(bool valid, float tempC, int hum, int wmoCode, float windKmh);
void facePomoData(int secLeft, bool running, bool isBreak, int roundsDone);

// ───────────────────── টাইমার ─────────────────────
// পমোডোরো ২৫ মিনিটেই বাঁধা; টাইমারটা নিজের ইচ্ছেমতো —
// টাইমারের পর্দায় টাচ ২ ছুঁয়ে সময় বসানো হয়।
enum TimerMode {
  TM_IDLE,     // কিছু বসানো হয়নি
  TM_SET,      // সময় বসাচ্ছি — এখন প্রতি ছোঁয়ায় +৫ মিনিট
  TM_RUN,      // গুনছে
  TM_PAUSE,    // থামানো, কিন্তু সময় জমা আছে
  TM_DONE      // শেষ — বিপ বেজেছে, পর্দা জ্বলছে-নিভছে
};

// secLeft: বাকি সেকেন্ড | totalSec: শুরুতে যা বসানো ছিল (বারের জন্য)
// setMin : TM_SET ভঙ্গিতে এখন কত মিনিট দেখাচ্ছে
// blink  : TM_DONE-এ এই ডাকে সংখ্যাটা দেখাব কি না
void faceTimerData(int secLeft, int totalSec, int setMin,
                   TimerMode mode, bool blink);

// ───────────────── নিচের লাইনের লেখা ─────────────────
// ⚠️ ক্রমটা banglabmp.h-এর BN_MSG তালিকার সাথে হুবহু মিলতে হবে
// (gen_bangla.py-র MSGS তালিকা থেকে দুটোই তৈরি)
enum FaceMsg {
  MSG_NONE, MSG_BOLUN, MSG_SHUNCHHI, MSG_BHABCHHI, MSG_BOLCHHI,
  MSG_JUKTECHHI, MSG_KOTA, MSG_WIFI_NEI, MSG_KEY_NEI, MSG_SEC_POR,
  MSG_SOMOSSA, MSG_OPEKKHA
};

void faceSetMsg(FaceMsg m);
void faceSetWait(int seconds);        // "৩০০ সেকেন্ড পর"

// ⭐ কথার সাথে ঠোঁট — level 0..255, অডিওর জোর।
// শুধু ঠোঁটের জানালাটুকু পাঠায়, তাই খুব সস্তা।
void faceMouth(uint8_t level);

// শোনার সময় মাইকের লেভেল দেখায় (একই সস্তা জানালা)
void faceMicLevel(uint8_t level);

// loop() থেকে ডাকুন — চোখের পলক, ভাবনার বিন্দু, এসব এখানে হয়
void faceTick();

// ── শুধু টেস্টের জন্য ──
// পিসিতে টেস্ট চালানোর সময় ছবির বাফারটা পড়তে দিই, যাতে সত্যিই
// ঠোঁট বড়-ছোট হচ্ছে কি না মেপে দেখা যায়। ফার্মওয়্যারে এটা থাকে না।
#ifdef FACE_TEST_HOOKS
const uint8_t *faceBuffer();          // 128*8 byte
#endif
