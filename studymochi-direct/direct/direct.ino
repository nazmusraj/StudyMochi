// ════════════════════════════════════════════════════════════════
//   স্টাডিমোচি DIRECT — ল্যাপটপ ছাড়া, ESP32 নিজেই গুগলে।
//
//        ESP32 ──── WiFi ────► Gemini Live API (wss)
//          ▲                          │
//          └────── অডিও ফেরত ─────────┘
//
//   ⚠️ এটা আলাদা স্কেচ। আপনার চলতি studymochi_esp32 কোড অটুট আছে —
//      এখানে কিছু ভাঙলেও ওটা আগের মতোই কাজ করবে।
//
//   ▸ লাইব্রেরি: WiFiManager (tzapu)  — এই একটাই লাগবে
//     (WebSocket নিজেরাই লিখেছি, miniws.cpp দেখুন)
//
//   ▸ প্রথমবার চালু হলে হটস্পট খুলবে: StudyMochi-Direct / mochi1234
//     সেখানে WiFi + Gemini API key দিন। NVS-এ জমা থাকবে।
//
//   ▸ ওয়্যারিং — আপনার বোর্ডের মতোই:
//       INMP441 : SCK=33  WS=25  SD=32   VDD→3V3  L/R→GND
//       MAX98357A: BCLK=26 LRC=27 DIN=14  VIN→5V   GAIN→GND  SD→VIN
//       BOOT বাটন চেপে ধরে কথা বলুন
// ════════════════════════════════════════════════════════════════

#include <WiFi.h>
// ⚠️ Wire.h এখানে **অবশ্যই** থাকতে হবে, যদিও ব্যবহার হয় face.cpp-তে।
//    Arduino IDE লাইব্রেরি খোঁজে মূলত .ino ফাইলের #include দেখে।
//    শুধু face.cpp-তে লিখলে IDE Wire লাইব্রেরির পথটা যোগ করে না,
//    আর তখন "Wire.h: No such file or directory" আসে।
#include <Wire.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <mbedtls/base64.h>
#include "miniws.h"
#include "face.h"
#include "touch.h"
#include "rtcclock.h"
#include "weather.h"

// ───────────────────── সেটিং ─────────────────────
#define AP_NAME    "StudyMochi-Direct"
#define AP_PASS    "mochi1234"

#define MIC_SCK    33
#define MIC_WS     25
#define MIC_SD     32
#define AMP_BCLK   26
#define AMP_LRC    27
#define AMP_DIN    14
// ───── OLED ─────
// আপনার ল্যাপটপ-ফার্মওয়্যারের মতোই পিন। OLED না লাগালেও কোড
// চলবে — faceBegin() false দেবে, আর মুখের সব ফাংশন চুপচাপ
// কিছু না করে ফিরে যাবে।
#define OLED_SDA   21
#define OLED_SCL   22
#define OLED_ADDR  0x3C

// ───── টাচ সেন্সর (TTP223 × ২) ─────
// প্রতিটায় ৩টা তার:  VCC→3V3   GND→GND   OUT→নিচের GPIO
//
//   টাচ-১ (GPIO 18) : চেপে ধরে কথা বলুন — BOOT বাটনের মতোই
//   টাচ-২ (GPIO 19) : একবার ছুঁলে পর্দা বদলায়
//                     মুখ → ঘড়ি → আবহাওয়া → পমোডোরো
//                     পমোডোরোর পর্দায় **চেপে ধরলে** টাইমার চালু/বন্ধ
//
// মডিউল উল্টো হলে (ছুঁলে LOW) touch.h-এ TOUCH_ACTIVE_LOW 1 করুন।
#define TOUCH_TALK 18
#define TOUCH_MENU 19

// ───── পমোডোরো ─────
// ───── টাইমার ─────
#define TMR_STEP_MIN      5        // এক চাপে কত মিনিট বাড়ে
#define TMR_MAX_MIN      60        // এর পরে ০ (মানে বাদ)
#define TMR_SET_WAIT_MS 3000       // বসানোর পর চুপ থাকলে কতক্ষণে চালু

#define POMO_WORK_SEC  (25 * 60)
#define POMO_BREAK_SEC (5 * 60)

// ───── আবহাওয়ার জায়গা (ঢাকা) ─────
// পোর্টাল থেকে বদলানো যায়
#define WX_LAT_DEFAULT 23.8103f
#define WX_LON_DEFAULT 90.4125f

#define BTN_PIN    0            // BOOT বাটন — কথা বলার জন্য (ব্যাকআপ)
#define LED_PIN    2

// ───── রিসেট বাটন ─────
// এক পা GPIO 4-এ, আরেক পা GND-তে। ভেতরের pull-up ব্যবহার হয়,
// তাই রেজিস্টর লাগবে না। বাটন না লাগালেও কোড চলবে (পিন HIGH থাকবে)।
//
// ৩ সেকেন্ড চেপে ধরলে WiFi + API key মুছে সেটআপ মোডে ফিরে যাবে।
// LED দ্রুত জ্বলে-নিভে বুঝিয়ে দেবে কতটা এগিয়েছে।
#define RESET_PIN      4
#define RESET_HOLD_MS  3000

#define MIC_RATE   16000        // Live API-র নিয়ম
#define OUT_RATE   24000        // Live API যা ফেরত দেয়
// আপনার আসল রেকর্ডিং মেপে এই মান বেরিয়েছে।
// logs\esp32-in-20260906-*.wav — gain ১৬-তে rms ছিল −৫.৪ dBFS আর
// **২২.৮% স্যাম্পল কেটে গিয়েছিল**। কথার জন্য চাই rms ≈ −২০ dBFS।
// ১৫ dB কমাতে হবে, মানে গেইন ৫.৪ গুণ কম → ১৬ ÷ ৫.৪ ≈ ৩।
#define MIC_GAIN   3
#define MIC_HPF_HZ 120

// মাইক কতটা জোরে ধরবে সেটা এখন **নিজে নিজে** ঠিক হয়:
//  · কোনো স্যাম্পল CLIP_AT ছাড়ালে সাথে সাথে গেইন নামে (কেটে যাওয়ার
//    চেয়ে একটু চাপা ভালো — কাটা গেলে গলা চেনাই যায় না)
//  · পুরো টার্ন খুব চাপা হলে পরেরবার গেইন ওঠে
//  · যা ঠিক হলো তা NVS-এ থাকে, পরের বার আর খুঁজতে হয় না
#define CLIP_AT    20000        // −৪ dBFS — এর ওপরে গেলে বিপদ
#define AIM_LOUD   11000        // কেটে গেলে এখানে নামিয়ে আনি (−৯ dBFS)
#define AIM_RMS    3000         // টার্ন শেষে গড় আওয়াজ এখানে আনার চেষ্টা
                                // (−২০ dBFS — কথার জন্য এটাই আদর্শ)
#define GAIN_MIN   1
#define GAIN_MAX   4096

// একবারে কত অডিও পাঠাব। ১৬০০ স্যাম্পল = ঠিক ১০০ ms।
// আগে ২৫৬ স্যাম্পল (১৬ ms) করে পাঠাতাম — সেকেন্ডে ৬২টা আলাদা
// মেসেজ, প্রতিটায় নতুন String। সেটাই লাইন কাটার বড় কারণ ছিল।
#define CHUNK_SAMPLES  1600

#define GEM_HOST   "generativelanguage.googleapis.com"
#define GEM_PATH   "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent"
#define GEM_MODEL  "models/gemini-2.5-flash-native-audio-preview-12-2025"
#define GEM_VOICE  "Kore"

#define I2S_MIC    I2S_NUM_0
#define I2S_AMP    I2S_NUM_1
#define I2S_WAIT   pdMS_TO_TICKS(200)

// ───────────────────── অবস্থা ─────────────────────
static Preferences prefs;
static MiniWS ws;
static char    gApiKey[140] = "";
static bool    gReady   = false;      // setupComplete পেয়েছি
static bool    gTalking = false;      // এখন রেকর্ড হচ্ছে
static bool    gMicOk = false, gAmpOk = false;
static uint32_t gPlayed = 0;
static uint32_t gSentMs = 0;          // এই টার্নে কত ms অডিও গেল
static int32_t  gPeak   = 0;          // এই টার্নে মাইকের সর্বোচ্চ (গেইনের পরে)
static int32_t  gRawPeak = 0;         // গেইনের আগে, কাঁচা মান
static uint32_t gClips  = 0;          // কতবার কেটে গেল
static uint64_t gSumSq  = 0;          // rms হিসেবের জন্য
static uint32_t gNSamp  = 0;
static uint32_t gWaitSince = 0;       // activityEnd-এর পর অপেক্ষা শুরু
static bool     gGotAudio = false;    // এই টার্নে উত্তরে অডিও এসেছে কি
static uint32_t gTurnAudio = 0;       // এই টার্নে মোট কত বাইট বাজল
static uint32_t gTurnT0 = 0;          // উত্তর আসা শুরু হয়েছিল কখন
static String   gHeard, gSaid;        // পুরো টার্নের লেখা, একবারে ছাপব
static int32_t  gGain   = MIC_GAIN;   // চলতি গেইন (NVS-এ জমা থাকে)
static int32_t  gGainStart = MIC_GAIN; // এই টার্ন যেটা দিয়ে শুরু হয়েছিল

// ───── টাচ, ঘড়ি, আবহাওয়া, পমোডোরো ─────
static Touch    tTalk, tMenu;
static float    gLat = WX_LAT_DEFAULT, gLon = WX_LON_DEFAULT;
static uint32_t gClockTick = 0;         // সেকেন্ডে একবার পর্দা নতুন করে
static bool     gPomoRun = false, gPomoBreak = false;
static int      gPomoLeft = POMO_WORK_SEC;
static int      gPomoRounds = 0;
static uint32_t gPomoTick = 0;
static bool     gSpeakOnIdle = false;   // পমোডোরো শেষে মোচি কথা বলবে
static char     gSpeakWhat[160] = "";

// ───── টাইমার ─────
// পমোডোরো ২৫ মিনিটেই বাঁধা। টাইমারটা নিজের ইচ্ছেমতো —
// টাইমারের পর্দায় টাচ ২ ছুঁয়ে সময় বসানো হয়।
static TimerMode gTmrMode = TM_IDLE;
static int       gTmrLeft = 0;          // বাকি সেকেন্ড
static int       gTmrTotal = 0;         // শুরুতে যা বসানো ছিল
static int       gTmrSetMin = 0;        // বসানোর ভঙ্গিতে এখনকার মিনিট
static uint32_t  gTmrTick = 0;
static uint32_t  gTmrBlink = 0;         // শেষ হলে পর্দা জ্বলে-নেভে
static bool      gTmrBlinkOn = true;
static int       gTmrBeeps = 0;         // আর কতগুলো বিপ বাকি
static uint32_t  gTmrBeepAt = 0;

static int32_t rawBuf[256];
static int16_t pcmBuf[CHUNK_SAMPLES];
static size_t  pcmFill = 0;

// অডিও মেসেজ এখানেই বানাই — String নয়, তাই heap নড়ে না।
// prefix(70) + base64(4268) + suffix(4) + '\0'  ≈ 4343
// SDK data আগে, mimeType পরে পাঠায় — আমরাও তাই
static const char AUD_PRE[]  = "{\"realtime_input\":{\"audio\":{\"data\":\"";
static const char AUD_POST[] = "\",\"mimeType\":\"audio/pcm;rate=16000\"}}}";
static char msgBuf[4608];

// হাই-পাস ফিল্টার (DC + ৫০Hz হাম কাটে)
static float hpR = 0, hx1 = 0, hy1 = 0, hx2 = 0, hy2 = 0;
static inline float hpf(float v) {
  float o1 = v  - hx1 + hpR * hy1; hx1 = v;  hy1 = o1;
  float o2 = o1 - hx2 + hpR * hy2; hx2 = o1; hy2 = o2;
  return o2;
}

// এক স্যাম্পল: গেইন → লিমিটার → হাই-পাস → ক্ল্যাম্প।
// লিমিটারটাই আসল কথা — কেটে যাওয়া গলা Gemini চিনতে পারে না, তাই
// প্রথম যে স্যাম্পলটা বিপদসীমা ছোঁয়, তখনই গেইন নামিয়ে দিই।
static int16_t micSample(int32_t raw) {
  int32_t ra = raw < 0 ? -raw : raw;
  if (ra > gRawPeak) gRawPeak = ra;

  int32_t s = (int32_t)(((int64_t)raw * gGain) >> 16);
  int32_t a = s < 0 ? -s : s;
  if (a > CLIP_AT) {
    gClips++;
    int32_t ng = (int32_t)(((int64_t)gGain * AIM_LOUD) / a);
    if (ng < GAIN_MIN) ng = GAIN_MIN;
    if (ng < gGain) gGain = ng;
    s = (int32_t)(((int64_t)raw * gGain) >> 16);
  }

  s = (int32_t)hpf((float)s);
  if (s >  32767) s =  32767;
  if (s < -32768) s = -32768;
  a = s < 0 ? -s : s;
  if (a > gPeak) gPeak = a;
  gSumSq += (uint64_t)((int64_t)s * (int64_t)s);   // rms-এর জন্য
  gNSamp++;
  return (int16_t)s;
}

// টার্নে গড় আওয়াজ কত ছিল
static int32_t turnRms() {
  if (!gNSamp) return 0;
  return (int32_t)sqrt((double)(gSumSq / gNSamp));
}

// ───── আগে থেকে জানিয়ে রাখি (setup() এগুলো ডাকে) ─────
static void showHelp();
static void factoryReset(const char *why);
static void saveGain();
static void planRetry(const char *why, uint32_t ms);
static void turnReport();
static void beep(int hz, int ms, int amp);
static void tmrClear();                 // Serial-এর 'k' এর আগেই লাগে

// ───── কত পরে আবার সেশন খুলব ─────
// ⚠️ এটাই আগের সবচেয়ে বড় ভুল ছিল। সংযোগ কাটলেই আমি **২ সেকেন্ড**
// পরে নতুন সেশন খুলতাম। ফ্রি টিয়ারে পরপর নতুন সেশন খোলা যায় না —
// তাই সারাদিনে শত শত চেষ্টা কোটা শেষ করে দিত, আর তখন ল্যাপটপ
// ভার্সনও উত্তর পেত না। এখন ব্যর্থ হলে অপেক্ষা দ্বিগুণ হতে থাকে।
static uint32_t gNextTry = 0;         // এর আগে আর চেষ্টা করব না
static uint32_t gBackoff = 0;         // এখনকার অপেক্ষা (ms)
static uint32_t gLastWaitMsg = 0;

// ───────────────────── I2S ─────────────────────
static void fillPins(i2s_pin_config_t &p, int bck, int wsp, int dout, int din) {
  memset(&p, 0xFF, sizeof(p));          // সব -1, mck_io_num সহ
  p.bck_io_num = bck; p.ws_io_num = wsp;
  p.data_out_num = dout; p.data_in_num = din;
}

static bool micBegin() {
  i2s_config_t c = {};
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  c.sample_rate = MIC_RATE;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  c.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  c.dma_buf_count = 8; c.dma_buf_len = 256; c.use_apll = false;
  i2s_pin_config_t p; fillPins(p, MIC_SCK, MIC_WS, I2S_PIN_NO_CHANGE, MIC_SD);
  if (i2s_driver_install(I2S_MIC, &c, 0, NULL) != ESP_OK) return false;
  return i2s_set_pin(I2S_MIC, &p) == ESP_OK;
}

static bool ampBegin() {
  i2s_config_t c = {};
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  c.sample_rate = OUT_RATE;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  c.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  // ⚠️ ৮ নয়, ১৬ বাফার। কেন:
  // গুগল ৪০ ms করে টুকরো পাঠায়, কিন্তু নেট সবসময় সমান তালে আসে না।
  // ৮টা বাফার মানে মোটে ৮৫ ms জমা থাকে — একটা টুকরো একটু দেরি
  // করলেই DMA খালি, আর কানে সেটা ঘড়ঘড়ে/ঘোলাটে শোনায়।
  // ১৬টায় ১৭০ ms জমা থাকে, নেটের এই এদিক-ওদিক সয়ে যায়।
  // দাম: ৮ KB RAM। আমাদের ~৭০ KB ফাঁকা, তাই সমস্যা নেই।
  c.dma_buf_count = 16; c.dma_buf_len = 256;
  c.use_apll = false; c.tx_desc_auto_clear = true;
  i2s_pin_config_t p; fillPins(p, AMP_BCLK, AMP_LRC, AMP_DIN, I2S_PIN_NO_CHANGE);
  if (i2s_driver_install(I2S_AMP, &c, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_AMP, &p) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_AMP);
  return true;
}

// ── ভলিউম ──
// Gemini-র উত্তরের অডিও প্রায় সর্বোচ্চ জোরে আসে (peak ৩২৩২৩ / ৩২৭৬৭)।
// MAX98357A-তে GAIN পিন GND-তে মানে আরও ১২ dB। ছোট স্পিকারে সেটা
// অ্যামপ বা কোনকে সীমা ছাড়িয়ে দেয় — কানে "ঘোলাটে/ফাটা" লাগে।
// তাই বাজানোর আগে একটু কমিয়ে নিই। v কমান্ড দিয়ে বদলানো যায়।
static int gVol = 70;                    // ০–১০০

// এই লুপটা এমনিতেই প্রতিটা স্যাম্পল ছুঁয়ে যায়, তাই সাথে সাথে
// জোরটাও মেপে নিই — ঠোঁট নড়ানোর জন্য আলাদা খরচ লাগে না।
static uint8_t gEnv = 0;                 // ০..২৫৫

static void applyVol(uint8_t *b, size_t n) {
  int16_t *s = (int16_t *)b;             // pcm বাফার 4-বাইট aligned
  size_t m = n / 2;
  int32_t peak = 0;
  for (size_t i = 0; i < m; i++) {
    int32_t v = s[i];
    if (gVol < 100) { v = (v * gVol) / 100; s[i] = (int16_t)v; }
    if (v < 0) v = -v;
    if (v > peak) peak = v;
  }
  // কথার শীর্ষ সাধারণত পুরো মাপের ~অর্ধেক, তাই ২ গুণ করে ছড়িয়ে দিই
  int32_t e = peak * 2 / 129;            // 32767*2/129 ≈ 508 -> clamp
  if (e > 255) e = 255;
  // চট করে ওঠে, ধীরে নামে — নইলে ঠোঁট কাঁপতে থাকে
  gEnv = (e > gEnv) ? (uint8_t)e : (uint8_t)((gEnv * 3 + e) / 4);
}

static void speakerWrite(const uint8_t *d, size_t n) {
  if (!gAmpOk) return;
  size_t done = 0;
  uint32_t t0 = millis();
  while (done < n) {
    size_t w = 0;
    if (i2s_write(I2S_AMP, d + done, n - done, &w, I2S_WAIT) != ESP_OK) break;
    done += w;
    if (w == 0 && millis() - t0 > 500) break;   // জ্যাম — বোর্ড ঝোলাব না
  }
}

// ───────────────────── Live API: পাঠানো ─────────────────────
// ⚠️⚠️ নিচের JSON-গুলোর বানান নিজে থেকে "সুন্দর" করতে যাবেন না।
//
// ল্যাপটপের পাইথন কোড (google-genai SDK) যেটা দিয়ে সব ঠিকঠাক কাজ
// করছিল, সেটা তারে ঠিক কী পাঠায় — আমরা সেটা ধরে দেখেছি। ফল:
//
//   {"realtime_input":{"activityStart":{}}}          ← বাইরে snake_case,
//   {"client_content":{"turns":[...],"turnComplete":true}}   ভেতরে camelCase
//   setup-এর ভেতরে speechConfig / realtimeInputConfig-এর
//   নিচের ফিল্ডগুলোও snake_case: voice_config, voice_name,
//   automatic_activity_detection, language_codes
//
// আগে আমি সব camelCase করে পাঠাতাম ("realtimeInput")। handshake আর
// setup তাতে টিকে যেত, কিন্তু অডিওর মেসেজগুলো সার্ভার আমলে নিত না —
// তাই Gemini নীরবতা শুনত আর কিছু না বলেই turnComplete পাঠাত।
// এখন হুবহু SDK-র মতো, অক্ষরে অক্ষরে।
static bool sendSetup() {
  String s = F("{\"setup\":{\"model\":\"");
  s += GEM_MODEL;
  s += F("\",\"generationConfig\":{\"responseModalities\":[\"AUDIO\"],"
         "\"speechConfig\":{\"voice_config\":{\"prebuilt_voice_config\":"
         "{\"voice_name\":\"");
  s += GEM_VOICE;
  s += F("\"}}}},\"systemInstruction\":{\"parts\":[{\"text\":\""
         "Tumi Mochi, ekjon dhoirjoshil private tutor. Chatro BUET EEE-r. "
         "Uttor dao BANGLAY, sohoj mukher bhashay. Technical term "
         "ENGREJITEI bolo (curl, divergence, impedance). Age ek line-e "
         "sorasori uttor, tarpor byakhya. Math kothay bolo, LaTeX noy. "
         "Uttor 150 shobder bhitore rakho.\"}],\"role\":\"user\"},"
         "\"inputAudioTranscription\":{\"language_codes\":[\"bn-BD\",\"en-US\"],"
         "\"mode\":\"SMART\"},"
         "\"outputAudioTranscription\":{\"language_codes\":[\"bn-BD\",\"en-US\"],"
         "\"mode\":\"SMART\"},"
         "\"realtimeInputConfig\":{\"automatic_activity_detection\":"
         "{\"disabled\":true}}}}");
  Serial.printf("[api] setup pathacchi (%u byte)\n", s.length());
  return ws.sendText(s);
}

static bool sendActivity(bool start) {
  const char *m = start
    ? "{\"realtime_input\":{\"activityStart\":{}}}"
    : "{\"realtime_input\":{\"activityEnd\":{}}}";
  return ws.sendText(m, strlen(m));
}

// মাইক ছাড়াই একটা প্রশ্ন পাঠায় — Serial-এ  t <proshno>
// মোচি এতে মুখে উত্তর দিলে বোঝা যায় গুগল, স্পিকার, সব ঠিক আছে;
// সমস্যা শুধু মাইকের অডিওতে।
static bool sendTextTurn(const char *q) {
  String s = F("{\"client_content\":{\"turns\":[{\"parts\":[{\"text\":\"");
  for (const char *p = q; *p; p++) {          // JSON ভাঙতে পারে এমন অক্ষর বাদ
    if (*p == '"' || *p == '\\') continue;
    if ((unsigned char)*p < 0x20) continue;
    s += *p;
  }
  s += F("\"}],\"role\":\"user\"}],\"turnComplete\":true}}");
  Serial.printf("[api] proshno pathacchi: %s\n", q);
  return ws.sendText(s);
}

// ১০০ ms অডিও base64 করে পাঠায়। base64 সরাসরি msgBuf-এর ভেতরে
// লেখা হয় — আলাদা বাফার বা String লাগে না।
static bool sendAudioChunk(const int16_t *pcm, size_t samples) {
  const size_t pre  = sizeof(AUD_PRE)  - 1;
  const size_t post = sizeof(AUD_POST) - 1;
  size_t room = sizeof(msgBuf) - pre - post;      // '\0'-এর জায়গাও এর ভেতরে

  size_t outLen = 0;
  if (mbedtls_base64_encode((unsigned char *)msgBuf + pre, room, &outLen,
                            (const unsigned char *)pcm, samples * 2) != 0) {
    Serial.println("[api] base64 buffer chhoto — chunk baad");
    return false;
  }
  memcpy(msgBuf, AUD_PRE, pre);                   // base64-এর পরেই prefix বসাই
  memcpy(msgBuf + pre + outLen, AUD_POST, post);  // mbedtls-এর '\0' ঢেকে দিই
  return ws.sendText(msgBuf, pre + outLen + post);
}

// শেষে একটু নীরবতা — নইলে DMA-তে পড়ে থাকা টুকরোটা "টক" করে বাজে
static void speakerSilence() {
  if (!gAmpOk) return;
  static const uint8_t z[512] = {0};
  for (int i = 0; i < 4; i++) speakerWrite(z, sizeof(z));
}

// একটা বিপ — তার আর অ্যামপ ঠিক আছে কি না, এক সেকেন্ডে বলে দেয়
static void beep(int hz, int ms, int amp) {
  if (!gAmpOk) {
    Serial.println("[i2s] amp chalu nei — beep bajano gelo na");
    return;
  }
  const int total = (OUT_RATE * ms) / 1000;
  int16_t chunk[256];
  int done = 0;
  while (done < total) {
    int n = (total - done) < 256 ? (total - done) : 256;
    for (int i = 0; i < n; i++) {
      float t = (float)(done + i) / OUT_RATE;
      float env = 1.0f;                       // শুরু/শেষে মৃদু ফেড
      if (done + i < 400)           env = (done + i) / 400.0f;
      if (total - (done + i) < 400) env = (total - (done + i)) / 400.0f;
      chunk[i] = (int16_t)(amp * env * sinf(2.0f * PI * hz * t));
    }
    applyVol((uint8_t *)chunk, n * sizeof(int16_t));
    speakerWrite((uint8_t *)chunk, n * sizeof(int16_t));
    done += n;
  }
  speakerSilence();
}

// টার্ন শেষে একবারেই সব খবর — অডিও বাজার সময় Serial চুপ থাকে
static void turnReport() {
  speakerSilence();
  float sec  = gTurnAudio / (2.0f * OUT_RATE);          // কত সেকেন্ড কথা
  float wall = (millis() - gTurnT0) / 1000.0f;          // আসতে কত লাগল
  Serial.printf("[api] turnComplete — %u byte, %.1fs audio, %.1fs-e elo\n",
                (unsigned)gTurnAudio, sec, wall);
  if (gHeard.length()) { Serial.print("[shunlam] "); Serial.println(gHeard); }
  if (gSaid.length())  { Serial.print("[mochi] ");   Serial.println(gSaid); }
  if (!gAmpOk)
    Serial.println("[i2s] ⚠ AMP CHALU NEI — audio ashche kintu bajche na!");
  else if (sec > 0.2f && wall > sec * 1.25f)
    Serial.printf("[i2s] ⚠ NET DHIME: %.1fs audio aste %.1fs lagchhe (%.0f%% dheri)\n"
                  "      -> sound ghorghore/gholate lagbe. WiFi-r kachhe jan.\n",
                  sec, wall, 100.0f * (wall / sec - 1.0f));
  gTurnAudio = 0; gHeard = ""; gSaid = "";
  faceSetState(FACE_IDLE);
  faceSetMsg(MSG_BOLUN);
}

// ───────────────────── Live API: গ্রহণ ─────────────────────
// পুরো ফ্রেম RAM-এ না রেখে বাইট-বাই-বাইট পড়ি। "data":"..." পেলে
// base64 ডিকোড করে সাথে সাথেই I2S-এ পাঠিয়ে দিই।
static void handleServerFrame(uint64_t len) {
  (void)len;
  // ছোট ছোট জিনিস খোঁজার জন্য একটা স্লাইডিং জানালা
  char win[24] = {0};
  int  wl = 0;

  bool inData = false;          // "data":" এর ভেতরে আছি
  bool sawInline = false;       // inlineData দেখেছি (অডিও, টেক্সট নয়)
  char q[4]; int qn = 0;        // base64-এর ৪ অক্ষর জমে
  uint8_t pcm[768] __attribute__((aligned(4))); size_t pn = 0;
  uint32_t audioBytes = 0;
  String textOut;               // transcription (ছোট)
  bool inText = false;
  bool inputTx = false;         // এখনকার transcription-টা কার — আমার না মোচির
  bool textIsInput = false;

  // ⚠️ আগে আমি হুবহু "data":" আর "text":" খুঁজতাম — মাঝে একটাও
  //    ফাঁকা জায়গা থাকলে চিনতাম না। JSON-এ ' "data" : " ' লেখা
  //    সমান বৈধ। ফলে turnComplete ধরা পড়ত (ওটা নিছক শব্দ, কোট
  //    লাগে না) কিন্তু অডিও আর কথা দুটোই হাতছাড়া হয়ে যেত —
  //    ঠিক যা আপনার বোর্ডে হচ্ছিল। এখন key পাওয়ার পর ':' আর
  //    '"'-এর মাঝের ফাঁকা জায়গা টপকে যাই।
  enum { W_NONE, W_DATA, W_TEXT } waitVal = W_NONE;
  int waitStage = 0;            // 0 = ':' খুঁজছি, 1 = খোলা '"' খুঁজছি
  bool turnDone = false;

  // ফ্রেমের শুরুর দিকটা আলাদা করে রাখি — সার্ভার কোনো error পাঠালে
  // এখানেই থাকবে, আর তখন আমরা সেটা ছাপিয়ে দিতে পারব।
  char head[200]; size_t hn = 0;

  auto flush = [&]() { if (pn) { applyVol(pcm, pn);
                               speakerWrite(pcm, pn);
                               faceMouth(gEnv);        // ⭐ ঠোঁট নড়ে
                               audioBytes += pn; pn = 0; } };

  while (true) {
    int c = ws.readByte();
    if (c < 0) break;

    if (inData) {
      if (c == '"') {                        // অডিও শেষ
        inData = false; qn = 0; flush();
        continue;
      }
      if (c == '\\') { ws.readByte(); continue; }
      q[qn++] = (char)c;
      if (qn == 4) {
        uint8_t out[3]; size_t on = 0;
        if (mbedtls_base64_decode(out, sizeof(out), &on,
                                  (const unsigned char *)q, 4) == 0) {
          for (size_t i = 0; i < on; i++) {
            pcm[pn++] = out[i];
            if (pn >= sizeof(pcm)) flush();
          }
        }
        qn = 0;
      }
      continue;
    }

    if (hn + 1 < sizeof(head)) head[hn++] = (char)c;

    if (inText) {
      if (c == '"') {
        inText = false;
        if (textOut.length()) {
          // ⚠️ এখানে ছাপি না — অডিও বাজার মাঝখানে Serial-এ লেখা
          //    মানে I2S-এর DMA খালি হয়ে যাওয়া, আর তখন শব্দ কেটে
          //    যায়। জমিয়ে রাখি, টার্ন শেষে একবারে ছাপব।
          String &box = textIsInput ? gHeard : gSaid;
          if (box.length() < 300) box += textOut;
          textOut = "";
        }
        continue;
      }
      if (c == '\\') { ws.readByte(); continue; }
      if (textOut.length() < 220) textOut += (char)c;
      continue;
    }

    // ── key পেয়েছি, এখন তার মানটার শুরু খুঁজছি ──
    if (waitVal != W_NONE) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
      if (waitStage == 0) {
        if (c == ':') { waitStage = 1; continue; }
        waitVal = W_NONE;                     // ভুল করে ধরেছিলাম
      } else {
        if (c == '"') {
          if (waitVal == W_DATA) { inData = true; sawInline = false; qn = 0; }
          else { inText = true; textIsInput = inputTx; textOut = ""; }
          waitVal = W_NONE; wl = 0; win[0] = 0;
          continue;
        }
        waitVal = W_NONE;                     // null ইত্যাদি — ছেড়ে দিই
      }
    }

    // ── জানালা সরাই ──
    if (wl < (int)sizeof(win) - 1) win[wl++] = (char)c;
    else { memmove(win, win + 1, sizeof(win) - 2); win[sizeof(win) - 2] = (char)c; }
    win[wl < (int)sizeof(win) ? wl : (int)sizeof(win) - 1] = 0;

    if (strstr(win, "inlineData")) { sawInline = true; wl = 0; win[0] = 0; }
    else if (strstr(win, "setupComplete")) {
      gReady = true;
      Serial.println("[api] setupComplete — SESSION READY");
      wl = 0; win[0] = 0;
    }
    else if (strstr(win, "turnComplete")) {
      gWaitSince = 0;
      if (gGotAudio) {
        turnDone = true;                 // ছাপাছাপি ফ্রেম শেষ হলে
      } else {
        // মডেল টার্ন শেষ বলল কিন্তু একটা শব্দও বলল না — মানে সে
        // যা শুনেছে তাতে কথা খুঁজে পায় নি।
        Serial.println("[api] turnComplete — kintu KONO UTTOR DEY NI.");
        Serial.println("      upore [shunlam] line achhe ki?");
        Serial.println("        · achhe  -> Gemini kotha shuneche, uttor dey ni");
        Serial.println("        · nei    -> audio-te kotha khunje pay ni (mic)");
        Serial.println("      Serial-e likhun:  t Coulomb er sutro ki");
        Serial.println("      mukhe uttor ele bujhben API+speaker thik, dosh mic-e.");
      }
      wl = 0; win[0] = 0;
    }
    else if (strstr(win, "interrupted")) { wl = 0; win[0] = 0; }
    else if (strstr(win, "inputTranscription"))  { inputTx = true;  wl = 0; win[0] = 0; }
    else if (strstr(win, "outputTranscription")) { inputTx = false; wl = 0; win[0] = 0; }
    else if (strstr(win, "\"text\"")) {
      waitVal = W_TEXT; waitStage = 0; wl = 0; win[0] = 0;
    }
    else if (sawInline && strstr(win, "\"data\"")) {
      waitVal = W_DATA; waitStage = 0; wl = 0; win[0] = 0;
    }
  }
  flush();
  ws.endFrame();
  head[hn] = 0;

  // সার্ভার কোনো ভুল ধরিয়ে দিলে সেটা চোখে পড়া দরকার
  if (strstr(head, "error") || strstr(head, "Error") ||
      strstr(head, "INVALID") || strstr(head, "PERMISSION")) {
    Serial.print("[api] SERVER BOLLO: ");
    Serial.println(head);
  }

  if (audioBytes) {
    gWaitSince = 0;
    if (!gGotAudio) { gTurnT0 = millis();
                      faceSetState(FACE_SPEAKING); faceSetMsg(MSG_BOLCHHI); }
    gGotAudio = true;
    gPlayed += audioBytes;
    gTurnAudio += audioBytes;
    // ⚠️ এখানে ছাপি না। প্রতি ফ্রেমে একটা লাইন মানে ২২৪টা লাইন,
    //    ১১৫২০০ baud-এ প্রায় ০.৭ সেকেন্ড — ততক্ষণ I2S-এর DMA খালি
    //    পড়ে থাকে আর শব্দ কেটে যায়।
  }
  if (textOut.length()) {                 // ফ্রেমের মাঝপথে কাটা পড়েছিল
    String &box = textIsInput ? gHeard : gSaid;
    if (box.length() < 300) box += textOut;
  }

  if (turnDone) turnReport();
}

static void pumpWs() {
  uint8_t op; uint64_t len;
  while (ws.beginFrame(op, len, 5)) {
    if (op == 0x1 || op == 0x2) {
      handleServerFrame(len);
    } else if (op == 0x8) {
      // ── সার্ভার কেন লাইন কাটল, সেটা এখন আমরা পড়ি ──
      uint16_t code = 0;
      char why[200];
      ws.readClose(len, code, why, sizeof(why));
      Serial.printf("[ws] server line kete dilo — code %u\n", (unsigned)code);
      if (why[0]) { Serial.print("[ws] KARON: "); Serial.println(why); }
      else        Serial.println("[ws] karon kichhu bole ni");

      bool quota = (code == 1011) || strstr(why, "quota") || strstr(why, "Quota")
                || strstr(why, "RESOURCE_EXHAUSTED");
      if (quota) {
        Serial.println("");
        Serial.println("  ┌──────────────────────────────────────────────┐");
        Serial.println("  │  QUOTA SHESH — Google notun session dicche na │");
        Serial.println("  │  eta code-er bhul noy.                       │");
        Serial.println("  │  barbar cheshta korle quota aro deri kore    │");
        Serial.println("  │  phire ase. tai 5 minute chup kore thakchi.  │");
        Serial.println("  └──────────────────────────────────────────────┘");
        Serial.println("");
        faceSetMsg(MSG_KOTA);
        planRetry("quota", 300000);
      }

      ws.stop();
      gReady = false;
      gTalking = false;
      pcmFill = 0;
      digitalWrite(LED_PIN, LOW);
      return;
    } else {
      ws.handleControl(op, len);
    }
  }
}

// ───────────────────── সেশন খোলা ─────────────────────
static bool openSession() {
  char path[400];
  snprintf(path, sizeof(path), "%s?key=%s", GEM_PATH, gApiKey);
  // SDK key-টা হেডারে পাঠায়; আমরা URL-এও রাখছি, দুটোই একই key
  char hdr[200];
  snprintf(hdr, sizeof(hdr), "x-goog-api-key: %s", gApiKey);
  if (!ws.connect(GEM_HOST, 443, path, hdr)) return false;

  gReady = false;
  if (!sendSetup()) { ws.stop(); return false; }

  uint32_t t0 = millis();
  while (!gReady && ws.connected() && millis() - t0 < 15000) { pumpWs(); delay(5); }
  if (!gReady) {
    Serial.println("[api] setupComplete pai ni — API key thik achhe ki?");
    ws.stop();
    return false;
  }
  return true;
}

// ───────────────────── বাটন ─────────────────────
static bool btnDown() { return digitalRead(BTN_PIN) == LOW; }

// ───────────────────── setup ─────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n\n=== StudyMochi DIRECT (laptop chhara) ===");
  Serial.printf("[chip] %s, free heap %u KB\n",
                ESP.getChipModel(), ESP.getFreeHeap() / 1024);

  // panel-er dhoron NVS-e jomano thake; 'o' command diye bodlano jay
  prefs.begin("mochidirect", true);
  bool sh1106 = prefs.getBool("sh1106", true);
  prefs.end();
  faceSetPanel(sh1106);
  bool oled = faceBegin(OLED_SDA, OLED_SCL, OLED_ADDR);
  Serial.printf("[oled] %s\n", oled ? "OK (SDA 21, SCL 22)"
                                     : "pai ni — OLED chhara-i cholbe");

  tTalk.begin(TOUCH_TALK);
  tMenu.begin(TOUCH_MENU);
  clockBegin();

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);      // বাটন না থাকলেও নিরাপদ (HIGH থাকবে)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  hpR = expf(-2.0f * PI * MIC_HPF_HZ / MIC_RATE);
  gMicOk = micBegin();
  gAmpOk = ampBegin();
  Serial.printf("[i2s] mic %s | amp %s\n", gMicOk ? "OK" : "BYARTHO",
                gAmpOk ? "OK" : "BYARTHO");

  // ── NVS থেকে API key ──
  prefs.begin("mochidirect", true);
  String k = prefs.getString("key", "");
  gGain = prefs.getInt("gain", MIC_GAIN);
  gVol  = prefs.getInt("vol", 70);
  gLat  = prefs.getFloat("lat", WX_LAT_DEFAULT);
  gLon  = prefs.getFloat("lon", WX_LON_DEFAULT);
  prefs.end();
  if (gGain < GAIN_MIN || gGain > GAIN_MAX) gGain = MIC_GAIN;
  if (gVol < 0 || gVol > 100) gVol = 70;
  gGainStart = gGain;
  strncpy(gApiKey, k.c_str(), sizeof(gApiKey) - 1);
  Serial.printf("[mic] gain %d (nije nije thik hoye jabe)\n", (int)gGain);

  // ── WiFi + key (পোর্টাল) ──
  WiFiManager wm;
  WiFiManagerParameter pKey("key", "Gemini API key", gApiKey,
                            sizeof(gApiKey) - 1);
  char latBuf[16], lonBuf[16];
  snprintf(latBuf, sizeof(latBuf), "%.4f", gLat);
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", gLon);
  WiFiManagerParameter pLat("lat", "Abohawa: latitude (Dhaka 23.8103)",
                            latBuf, sizeof(latBuf) - 1);
  WiFiManagerParameter pLon("lon", "Abohawa: longitude (Dhaka 90.4125)",
                            lonBuf, sizeof(lonBuf) - 1);
  wm.addParameter(&pKey);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.setConfigPortalTimeout(240);
  wm.setDarkMode(true);
  wm.setTitle("StudyMochi Direct");

  // ── পোর্টাল কখন খুলবে ──
  // ⚠️ ESP32-এর ভেতরে আগের স্কেচের WiFi পাসওয়ার্ড জমা থাকে, তাই
  //    autoConnect() সোজা জুড়ে যায় আর পোর্টাল খোলেই না। তখন API key
  //    দেওয়ার সুযোগই থাকে না। তাই **key না থাকলে জোর করে পোর্টাল**।
  bool needKey = strlen(gApiKey) < 10;

  // বুটের পর বাটন চেপে ধরলেও পোর্টাল খুলবে (key বদলানোর জন্য)
  bool force = false;
  uint32_t held = millis();
  while (btnDown()) { if (millis() - held > 1500) { force = true; break; } delay(20); }

  bool ok;
  if (needKey || force) {
    Serial.println("\n╔════════════ SETUP MODE ════════════");
    if (needKey) Serial.println("║ API key nei — tai portal khulchi");
    Serial.println("║");
    Serial.printf ("║ 1) phone-e WiFi settings kholun\n");
    Serial.printf ("║ 2) juktun :  %s\n", AP_NAME);
    Serial.printf ("║    password:  %s\n", AP_PASS);
    Serial.println("║ 3) page nije khule jabe");
    Serial.println("║    (na khule browser-e 192.168.4.1)");
    Serial.println("║ 4) \"Configure WiFi\" chapun");
    Serial.println("║ 5) nijer WiFi bachun + password din");
    Serial.println("║ 6) SEI PAGE-EI niche 'Gemini API key' ghor");
    Serial.println("║    -> sekhane key paste korun");
    Serial.println("║ 7) Save");
    Serial.println("╚════════════════════════════════════\n");
    faceSetState(FACE_PORTAL);
    ok = wm.startConfigPortal(AP_NAME, AP_PASS);
  } else {
    Serial.println("[wifi] jana WiFi-te juktechi...");
    ok = wm.autoConnect(AP_NAME, AP_PASS);
  }

  if (!ok) { Serial.println("[wifi] jukte parlam na - restart"); delay(2000); ESP.restart(); }

  // ── পোর্টালে দেওয়া key জমা রাখি ──
  if (strlen(pKey.getValue()) > 10) {
    strncpy(gApiKey, pKey.getValue(), sizeof(gApiKey) - 1);
    gApiKey[sizeof(gApiKey) - 1] = 0;
    prefs.begin("mochidirect", false);
    prefs.putString("key", gApiKey);
    prefs.end();
    Serial.println("[nvs] API key save holo");
  }
  // আবহাওয়ার জায়গা — পোর্টালে দেওয়া থাকলে জমা রাখি
  {
    float la = atof(pLat.getValue()), lo = atof(pLon.getValue());
    if (la >= -90 && la <= 90 && lo >= -180 && lo <= 180 &&
        (la != 0 || lo != 0)) {
      gLat = la; gLon = lo;
      prefs.begin("mochidirect", false);
      prefs.putFloat("lat", gLat);
      prefs.putFloat("lon", gLon);
      prefs.end();
      Serial.printf("[wx] jayga: %.4f, %.4f\n", gLat, gLon);
    }
  }

  Serial.print("[wifi] OK, IP "); Serial.println(WiFi.localIP());

  // WiFi পাওয়া গেছে — ঘড়িটা NTP থেকে মিলিয়ে নিই (বাংলাদেশ UTC+6)।
  // না পেলে কম্পাইলের সময়টা বসাই, যাতে ঘড়ি অন্তত চলে।
  if (clockOk()) {
    if (!clockSyncNTP(6 * 3600)) {
      MochiTime t = clockNow();
      if (!t.valid) clockSetFromBuild();
    }
    MochiTime t = clockNow();
    Serial.printf("[rtc] ekhon %02d:%02d:%02d  %02d/%02d/%04d\n",
                  t.hour24, t.minute, t.second, t.day, t.month, t.year);
  }
  weatherFetch(gLat, gLon);        // প্রথমবার এনে রাখি
  if (strlen(gApiKey) < 10) {
    Serial.println("\n[api] EKHONO API KEY NEI.");
    Serial.println("      Serial Monitor-e  p  likhe ENTER chapun");
    Serial.println("      -> portal abar khulbe.");
    faceSetMsg(MSG_KEY_NEI);
    showHelp();
    return;
  }
  Serial.printf("[api] key achhe (%u okkhor)\n", (unsigned)strlen(gApiKey));

  showHelp();
  if (openSession()) {
    Serial.println("\n>>> BOOT BOTAM CHEPE DHORE KOTHA BOLUN <<<\n");
    faceSetState(FACE_IDLE);
    faceSetMsg(MSG_BOLUN);
  } else {
    planRetry("prothom cheshta byartho", 15000);
  }
}

// ───────────────────── loop ─────────────────────
// ───── Serial Monitor-এর কমান্ড ─────
//   p  → পোর্টাল খোলো (WiFi ও key বদলাও, পুরোনোটা রেখে)
//   r  → সব মুছে ফেলো (WiFi + API key) আর নতুন করে শুরু
//   i  → এখনকার অবস্থা দেখাও
// WiFi ও API key দুটোই মুছে নতুন করে শুরু।
// Serial-এর 'r' আর GPIO 4-এর বাটন — দুটোই এখানে আসে।
static void factoryReset(const char *why) {
  Serial.printf("\n[reset] %s — SOB MUCHE DICCHI (WiFi + API key)...\n", why);
  ws.stop();
  prefs.begin("mochidirect", false);
  prefs.clear();                                 // API key মুছি
  prefs.end();
  WiFiManager wm;
  wm.resetSettings();                            // WiFi পাসওয়ার্ড মুছি
  Serial.printf("[reset] muche gechhe. phone diye '%s' hotspot-e jurun\n", AP_NAME);
  for (int i = 0; i < 6; i++) {                  // LED দিয়ে সংকেত
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);  delay(80);
  }
  delay(400);
  ESP.restart();
}

static void showHelp() {
  Serial.println("\n  ── Serial command ──");
  Serial.println("   o + ENTER   : OLED SH1106 <-> SSD1306 bodlao");
  Serial.println("  ── touch ──");
  Serial.printf ("   GPIO %d chepe dhorun : kotha bolun\n", TOUCH_TALK);
  Serial.printf ("   GPIO %d CHEPE DHORLE : porda bodlay\n", TOUCH_MENU);
  Serial.println("     mukh > ghori > abohawa > pomodoro > timer");
  Serial.printf ("   GPIO %d EK CHAP     : oi porda-r kaj\n", TOUCH_MENU);
  Serial.println("     pomodoro : chalu / bondho");
  Serial.println("     timer    : 1 chap = bosano shuru, ar chap = +5 min");
  Serial.println("                (60-er por 0 = bad). 3s chup thakle chalu.");
  Serial.println("                chole thakle: chap = thamao / abar chalu");
  Serial.println("     abohawa  : notun kore khobor ane");
  Serial.println("   s + ENTER   : speaker beep — tar thik achhe ki");
  Serial.println("   v <0-100>   : speaker volume (blurry hole koman)");
  Serial.println("   t <proshno> : mic chhara likhe proshno korun");
  Serial.println("                 (mukhe uttor ele API+speaker thik)");
  Serial.println("   g <number>  : mic gain hate bodlan (g = ekhonkar man)");
  Serial.println("   p + ENTER : portal kholo (WiFi/key bodlao)");
  Serial.println("   r + ENTER : SOB MUCHE dao (WiFi + API key)");
  Serial.println("   k + ENTER : timer muchhe dao");
  Serial.println("   i + ENTER : ekhonkar obostha");
  Serial.printf ("   othoba GPIO %d-er botam %us chepe dhorun\n",
                 RESET_PIN, RESET_HOLD_MS / 1000);
  Serial.println("  ────────────────────\n");
}

static void checkSerialCmd() {
  if (!Serial.available()) return;

  // পুরো লাইনটা পড়ি — তাহলে  t <proshno>  আর  g 8  লেখা যায়
  char line[96]; size_t n = 0;
  bool done = false;
  uint32_t t0 = millis();
  while (!done && millis() - t0 < 80) {
    while (Serial.available()) {
      char ch = (char)Serial.read();
      t0 = millis();
      if (ch == '\n' || ch == '\r') { if (n) { done = true; break; } continue; }
      if (n + 1 < sizeof(line)) line[n++] = ch;
    }
    if (!done) delay(2);
  }
  line[n] = 0;
  if (!n) return;

  char c = line[0];
  const char *rest = line + 1;
  while (*rest == ' ') rest++;

  if (c == 'o' || c == 'O') {
    bool now = !faceIsSH1106();
    faceSetPanel(now);
    prefs.begin("mochidirect", false);
    prefs.putBool("sh1106", now);
    prefs.end();
    Serial.printf("[oled] ekhon %s dhore nilam. porda thik hoyeche?\n"
                  "       na hole abar  o  chapun.\n",
                  now ? "SH1106 (1.3 inch)" : "SSD1306 (0.96 inch)");
    return;
  }

  if (c == 's' || c == 'S') {
    Serial.println("[test] beep bajachhi — speaker theke shunte pachhen?");
    beep(440, 400, 7000);
    Serial.println("[test] shesh. sound na pele: SD pin VIN-e achhe ki?"
                   " GAIN pin GND-te? VIN 5V-e?");
    return;
  }

  if (c == 'v' || c == 'V') {
    if (!*rest) {
      Serial.printf("[spk] ekhonkar volume %d.  bodlate:  v 50\n", gVol);
      return;
    }
    long v = atol(rest);
    if (v < 0 || v > 100) { Serial.println("[spk] 0 theke 100-er moddhe din"); return; }
    gVol = (int)v;
    prefs.begin("mochidirect", false);
    prefs.putInt("vol", gVol);
    prefs.end();
    Serial.printf("[spk] volume %d holo. beep diye shune dekhun:  s\n", gVol);
    return;
  }

  if (c == 't' || c == 'T') {
    if (!ws.connected() || !gReady) {
      Serial.println("[cmd] session ekhono ready noy — ektu opekkha korun");
      return;
    }
    const char *q = *rest ? rest : "Coulomb er sutro ki? Choto kore bolo.";
    gGotAudio = false;
    if (sendTextTurn(q)) gWaitSince = millis();
    return;
  }

  if (c == 'g' || c == 'G') {
    if (!*rest) {
      Serial.printf("[mic] ekhonkar gain %d.  bodlate:  g 8\n", (int)gGain);
      return;
    }
    long v = atol(rest);
    if (v < GAIN_MIN || v > GAIN_MAX) {
      Serial.printf("[mic] gain %d theke %d-er moddhe din\n", GAIN_MIN, GAIN_MAX);
      return;
    }
    gGain = (int32_t)v;
    gGainStart = gGain;
    saveGain();
    Serial.printf("[mic] gain %d kore dilam (NVS-e save holo)\n", (int)gGain);
    return;
  }

  if (c == 'i' || c == 'I') {
    Serial.printf("\n  WiFi     : %s  (%s)\n",
                  WiFi.isConnected() ? WiFi.SSID().c_str() : "juktona",
                  WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "-");
    Serial.printf("  API key  : %s\n", strlen(gApiKey) > 10 ? "achhe" : "NEI");
    Serial.printf("  session  : %s\n", ws.connected() ? (gReady ? "ready" : "khulche") : "bondho");
    Serial.printf("  mic/amp  : %s / %s\n", gMicOk ? "OK" : "BYARTHO", gAmpOk ? "OK" : "BYARTHO");
    Serial.printf("  sesh turn: %u ms pathano, mic peak %d, raw peak %d\n",
                  (unsigned)gSentMs, (int)gPeak, (int)gRawPeak);
    Serial.printf("  mic gain : %d\n", (int)gGain);
    Serial.printf("  spk vol  : %d\n", gVol);
    Serial.printf("  free RAM : %u KB\n", ESP.getFreeHeap() / 1024);
    {
      static const char *const TM[] = {"faka", "bosachhi", "cholchhe",
                                       "thamano", "SHESH"};
      Serial.printf("  timer    : %s", TM[(int)gTmrMode]);
      if (gTmrMode == TM_SET)       Serial.printf(" (%d min)", gTmrSetMin);
      else if (gTmrMode != TM_IDLE) Serial.printf(" (%d:%02d baki)",
                                                  gTmrLeft / 60, gTmrLeft % 60);
      Serial.println();
    }
    Serial.printf("  pomodoro : %s (%d:%02d baki, %d round)\n",
                  gPomoRun ? (gPomoBreak ? "biroti" : "porchhi") : "thamano",
                  gPomoLeft / 60, gPomoLeft % 60, gPomoRounds);
    showHelp();
    return;
  }

  if (c == 'k' || c == 'K') {
    gTmrBeeps = 0;
    tmrClear();
    Serial.println("[timer] muchhe dilam");
    return;
  }

  if (c == 'r' || c == 'R') {
    factoryReset("serial command");
    return;
  }

  if (c == 'p' || c == 'P') {
    Serial.println("[cmd] portal khulchi — phone diye StudyMochi-Direct-e jurun");
    ws.stop();
    WiFiManager wm;
    WiFiManagerParameter pKey("key", "Gemini API key", gApiKey,
                              sizeof(gApiKey) - 1);
    wm.addParameter(&pKey);
    wm.setDarkMode(true);
    wm.setConfigPortalTimeout(240);
    wm.startConfigPortal(AP_NAME, AP_PASS);
    if (strlen(pKey.getValue()) > 10) {
      strncpy(gApiKey, pKey.getValue(), sizeof(gApiKey) - 1);
      gApiKey[sizeof(gApiKey) - 1] = 0;
      prefs.begin("mochidirect", false);
      prefs.putString("key", gApiKey);
      prefs.end();
      Serial.println("[nvs] API key save holo — restart korchi");
      delay(500);
      ESP.restart();
    }
  }
}

// GPIO 4-এর বাটন ৩ সেকেন্ড চেপে ধরা হয়েছে কি — loop() থেকে ডাকা হয়
static void checkResetButton() {
  static uint32_t downSince = 0;
  static uint32_t lastBlink = 0;
  static bool     fired = false;      // একবার চললে বাটন না ছাড়া পর্যন্ত আর নয়

  if (digitalRead(RESET_PIN) != LOW) {            // ছাড়া আছে
    if (downSince && !fired) {                    // মাঝপথে ছেড়ে দিল
      Serial.println("[reset] batil kora holo");
      digitalWrite(LED_PIN, LOW);
    }
    downSince = 0;
    fired = false;                                // এবার আবার চাপা যাবে
    return;
  }

  if (fired) return;                              // ধরে রেখেছে — বারবার নয়

  uint32_t now = millis();
  if (!downSince) {
    downSince = now;
    Serial.printf("[reset] botam chepe achhe... %us dhore dhore rakhun\n",
                  RESET_HOLD_MS / 1000);
    return;
  }

  uint32_t held = now - downSince;

  // যত সময় যায় LED তত দ্রুত জ্বলে — কতটা এগিয়েছে বোঝা যায়
  uint32_t period = held > 2000 ? 80 : (held > 1000 ? 160 : 300);
  if (now - lastBlink > period) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if (held >= RESET_HOLD_MS) {
    fired = true;                                 // চেপে ধরে থাকলেও আর নয়
    factoryReset("GPIO 4 botam");
  }
}

// পরের চেষ্টা কখন — আর কেন, সেটা পরিষ্কার করে বলে দিই
static void planRetry(const char *why, uint32_t ms) {
  if (ms < 15000)  ms = 15000;         // ১৫ সেকেন্ডের কমে কখনো নয়
  if (ms > 300000) ms = 300000;        // ৫ মিনিটের বেশিও নয়
  gBackoff = ms;
  gNextTry = millis() + ms;
  gLastWaitMsg = 0;
  faceSetState(FACE_WAITING);
  faceSetWait((int)(ms / 1000));   // "৩০০ সেকেন্ড পর" — উপরে
  Serial.printf("[ws] %s — %u second por abar cheshta korbo\n",
                why, (unsigned)(ms / 1000));
}

static void saveGain() {
  prefs.begin("mochidirect", false);
  prefs.putInt("gain", gGain);
  prefs.end();
}

// এক চাঁক অডিও পাঠাই। না গেলে লাইনটা আর বিশ্বাস করি না।
static bool pushChunk() {
  if (!pcmFill) return true;
  size_t n = pcmFill;
  pcmFill = 0;
  if (!sendAudioChunk(pcmBuf, n)) {
    Serial.println("[api] audio pathano gelo na — session bondho");
    ws.stop();
    gReady = false;
    return false;
  }
  gSentMs += (uint32_t)(n / (MIC_RATE / 1000));
  return true;
}

// ───────────────────── টাচ, পর্দা, পমোডোরো ─────────────────────
// মোচিকে কিছু বলাতে চাই (পমোডোরো শেষ হলো ইত্যাদি) — কিন্তু সে
// যখন ফাঁকা আছে তখনই, নইলে চলতি উত্তরের মাঝখানে ঢুকে পড়বে।
static void askMochi(const char *what) {
  strncpy(gSpeakWhat, what, sizeof(gSpeakWhat) - 1);
  gSpeakWhat[sizeof(gSpeakWhat) - 1] = 0;
  gSpeakOnIdle = true;
}

static void pomoStart(bool brk) {
  gPomoBreak = brk;
  gPomoLeft  = brk ? POMO_BREAK_SEC : POMO_WORK_SEC;
  gPomoRun   = true;
  gPomoTick  = millis();
  facePomoData(gPomoLeft, gPomoRun, gPomoBreak, gPomoRounds);
  faceRedraw();
  Serial.printf("[pomo] %s shuru — %d minute\n",
                brk ? "biroti" : "porar somoy", gPomoLeft / 60);
}

static void pomoTick(uint32_t now) {
  if (!gPomoRun) return;
  if (now - gPomoTick < 1000) return;
  gPomoTick += 1000;
  if (gPomoLeft > 0) gPomoLeft--;

  if (gPomoLeft == 0) {
    gPomoRun = false;
    if (!gPomoBreak) {
      gPomoRounds++;
      Serial.printf("[pomo] %d nombor round shesh\n", gPomoRounds);
      askMochi("Amar 25 minute porar somoy shesh holo. "
               "Choto kore obhinondon jano ar 5 minute bishram nite bolo.");
      pomoStart(true);                       // সাথে সাথে বিরতি
    } else {
      Serial.println("[pomo] biroti shesh");
      askMochi("Bishram shesh. Amake abar porte bosar janno ek line-e utsaho dao.");
      gPomoLeft = POMO_WORK_SEC;             // পরের রাউন্ড হাতে শুরু হবে
    }
  }
  facePomoData(gPomoLeft, gPomoRun, gPomoBreak, gPomoRounds);
  if (faceScreen() == SCR_POMO) faceRedraw();
}

// ───────────────────── টাইমার ─────────────────────
static void tmrPush() {
  faceTimerData(gTmrLeft, gTmrTotal, gTmrSetMin, gTmrMode, gTmrBlinkOn);
  if (faceScreen() == SCR_TIMER && faceGetState() == FACE_IDLE) faceRedraw();
}

static void tmrStart(int minutes) {
  if (minutes < 1) minutes = 1;
  if (minutes > 180) minutes = 180;
  gTmrTotal = minutes * 60;
  gTmrLeft  = gTmrTotal;
  gTmrMode  = TM_RUN;
  gTmrTick  = millis();
  gTmrBeeps = 0;
  gTmrBlinkOn = true;
  tmrPush();
  Serial.printf("[timer] %d minute chalu\n", minutes);
}

static void tmrClear() {
  gTmrMode = TM_IDLE;
  gTmrLeft = gTmrTotal = gTmrSetMin = 0;
  gTmrBeeps = 0;
  gTmrBlinkOn = true;
  tmrPush();
}

static void tmrTick(uint32_t now) {
  // ── বাকি বিপগুলো ──
  // beep() নিজে ব্লক করে, তাই একসাথে তিনটে বাজাই না — একটা করে,
  // ৪৫০ ms পর পর। মাঝখানে loop() চলতে থাকে।
  if (gTmrBeeps > 0 && (int32_t)(now - gTmrBeepAt) >= 0) {
    beep(880, 220, 9000);
    gTmrBeeps--;
    gTmrBeepAt = now + 450;
  }

  // ── শেষ হয়ে গেলে পর্দা জ্বলে-নেভে ──
  if (gTmrMode == TM_DONE) {
    if (now - gTmrBlink >= 500) {
      gTmrBlink = now;
      gTmrBlinkOn = !gTmrBlinkOn;
      tmrPush();
    }
    return;
  }

  // ── বসানোর ভঙ্গিতে চুপ থাকলে নিজে থেকেই চালু ──
  // বোতাম একটাই, তাই "শুরু করো" বলার আলাদা উপায় নেই। সংখ্যাটা
  // পছন্দ হলে আঙুল সরিয়ে নিলেই তিন সেকেন্ড পর গোনা শুরু।
  if (gTmrMode == TM_SET) {
    if (gTmrSetMin > 0 && now - gTmrTick >= TMR_SET_WAIT_MS) tmrStart(gTmrSetMin);
    return;
  }

  if (gTmrMode != TM_RUN) return;
  if (now - gTmrTick < 1000) return;
  gTmrTick += 1000;
  if (gTmrLeft > 0) gTmrLeft--;

  if (gTmrLeft == 0) {
    gTmrMode    = TM_DONE;
    gTmrBeeps   = 3;                      // শুধু বিপ — কোনো কথা নয়,
    gTmrBeepAt  = now;                    // তাই নেট বা কোটা লাগে না
    gTmrBlink   = now;
    gTmrBlinkOn = true;
    Serial.printf("[timer] somoy shesh (%d minute)\n", gTmrTotal / 60);
  }
  tmrPush();
}

// টাইমারের পর্দায় এক চাপ — সব কাজ এই একটা ছোঁয়াতেই
static void tmrTap(uint32_t now) {
  switch (gTmrMode) {
    case TM_IDLE:
      gTmrMode   = TM_SET;
      gTmrSetMin = TMR_STEP_MIN;
      gTmrTick   = now;
      Serial.printf("[timer] bosachhi — %d minute\n", gTmrSetMin);
      break;

    case TM_SET:
      // ৫ → ১০ → … → ৬০ → ০ (০ মানে বাদ)
      gTmrSetMin += TMR_STEP_MIN;
      if (gTmrSetMin > TMR_MAX_MIN) gTmrSetMin = 0;
      gTmrTick = now;
      if (gTmrSetMin == 0) {
        Serial.println("[timer] bad dilam");
        gTmrMode = TM_IDLE;
      } else {
        Serial.printf("[timer] %d minute\n", gTmrSetMin);
      }
      break;

    case TM_RUN:
      gTmrMode = TM_PAUSE;
      Serial.printf("[timer] thamlo — baki %d:%02d\n",
                    gTmrLeft / 60, gTmrLeft % 60);
      break;

    case TM_PAUSE:
      gTmrMode = TM_RUN;
      gTmrTick = now;
      Serial.println("[timer] abar chalu");
      break;

    case TM_DONE:
      gTmrBeeps = 0;                      // বিপ থামাই
      tmrClear();
      Serial.println("[timer] muchhe dilam");
      return;
  }
  tmrPush();
}

// ⭐ টাচ ২ — উল্টো করা হয়েছে (আপনার কথায়):
//    চেপে ধরলে পর্দা বদলায়, এক চাপে সেই পর্দার কাজ হয়।
//    আগে উল্টোটা ছিল, তাতে কাজ করতে গিয়ে পর্দা বদলে যেত।
static void handleMenuTouch(uint32_t now) {
  // ── চেপে ধরা → পরের পর্দা ──
  if (tMenu.tookHold()) {
    faceNextScreen();
    FaceScreen sc = faceScreen();
    Serial.printf("[touch] porda: %s\n",
                  sc == SCR_FACE ? "mukh" : sc == SCR_CLOCK ? "ghori" :
                  sc == SCR_WEATHER ? "abohawa" :
                  sc == SCR_POMO ? "pomodoro" : "timer");
    if (sc == SCR_WEATHER) {
      // পর্দায় এলেই টাটকা খবর আনি (১৫ মিনিটের মধ্যে হলে ক্যাশ থেকেই)
      if (weatherFetch(gLat, gLon)) {
        WeatherNow w = weatherGet();
        faceWeatherData(true, w.tempC, w.humidity, w.code, w.windKmh);
      }
    }
    if (sc == SCR_TIMER) tmrPush();
    faceRedraw();
    return;                               // এক ছোঁয়ায় একটাই কাজ
  }

  // ── এক চাপ → এই পর্দার কাজ ──
  if (!tMenu.tookTap()) return;

  switch (faceScreen()) {
    case SCR_POMO:
      if (gPomoRun) {
        gPomoRun = false;
        Serial.println("[pomo] thamlo");
        facePomoData(gPomoLeft, gPomoRun, gPomoBreak, gPomoRounds);
        faceRedraw();
      } else {
        pomoStart(gPomoBreak);
      }
      break;

    case SCR_TIMER:
      tmrTap(now);
      break;

    case SCR_WEATHER:
      // আবার খবর আনি — ক্যাশ ফেলে দিয়ে
      Serial.println("[wx] notun kore anchhi");
      if (weatherFetch(gLat, gLon, true)) {
        WeatherNow w = weatherGet();
        faceWeatherData(true, w.tempC, w.humidity, w.code, w.windKmh);
      }
      faceRedraw();
      break;

    default:                              // মুখ, ঘড়ি — কিছু করার নেই
      break;
  }
}

// ঘড়ির পর্দা থাকলে সেকেন্ডে একবার নতুন করে আঁকি
static void clockTick(uint32_t now) {
  if (faceScreen() != SCR_CLOCK) return;
  if (faceGetState() != FACE_IDLE) return;      // মোচি কাজে থাকলে নয়
  if (now - gClockTick < 1000) return;
  gClockTick = now;
  MochiTime t = clockNow();
  faceClockData(t.hour24, t.minute, t.second, t.day, t.month, t.year,
                t.dow, clockOk() && t.valid);
  faceRedraw();
}

void loop() {
  uint32_t now = millis();
  tTalk.update(now);
  tMenu.update(now);
  handleMenuTouch(now);
  pomoTick(now);
  tmrTick(now);
  clockTick(now);

  checkSerialCmd();
  checkResetButton();
  faceTick();                      // chokher polok, bhabnar bindu
  if (strlen(gApiKey) < 10) { delay(1000); return; }

  // সংযোগ কেটে গেলে আবার খুলি — কিন্তু ধীরে সুস্থে
  if (!ws.connected()) {
    gTalking = false; pcmFill = 0; digitalWrite(LED_PIN, LOW);

    int32_t left = (int32_t)(gNextTry - millis());
    if (left > 0) {                       // এখনো সময় হয়নি
      if (millis() - gLastWaitMsg > 30000) {
        gLastWaitMsg = millis();
        Serial.printf("[ws] opekkha... aro %d second\n", left / 1000);
      }
      delay(50);
      return;                             // বাটন/serial তবু চলবে
    }

    Serial.println("[ws] session khulchi...");
    faceSetMsg(MSG_JUKTECHHI);
    if (!openSession()) {
      planRetry("khola gelo na", gBackoff ? gBackoff * 2 : 15000);
      return;
    }
    gBackoff = 0;                         // সফল — গোনা শুরু থেকে
    Serial.println(">>> BOOT BOTAM CHEPE DHORE KOTHA BOLUN <<<");
  }

  pumpWs();
  if (!ws.connected()) return;          // pumpWs লাইন কেটে দিয়ে থাকতে পারে

  // উত্তরের অপেক্ষা বেশি লম্বা হলে জানিয়ে দিই
  // ⚠️ ৩০ সেকেন্ড, ১৫ নয় — protocheck-এ দেখা গেল মডেল ভেবেচিন্তে
  //    উত্তর দিতে ২০ সেকেন্ডও নিতে পারে। আগে সেই স্বাভাবিক দেরিতেই
  //    "উত্তর এল না" লেখা ভেসে উঠত, অথচ উত্তর আসছিল।
  if (gWaitSince && millis() - gWaitSince > 30000) {
    gWaitSince = 0;
    Serial.println("[api] 30s dhore kono uttor elo na.");
    Serial.println("      mic peak dekhun — 500-er niche hole mic-e sound jacche na.");
  }

  // পমোডোরোর বার্তা জমে থাকলে, মোচি ফাঁকা হলেই বলে দিই
  if (gSpeakOnIdle && gReady && !gTalking && !gWaitSince) {
    gSpeakOnIdle = false;
    gGotAudio = false;
    faceSetState(FACE_THINKING);
    if (sendTextTurn(gSpeakWhat)) gWaitSince = millis();
  }

  bool down = btnDown() || tTalk.isDown();

  // ── বাটন চাপা হলো ──
  if (down && !gTalking && gReady) {
    gTalking = true;
    digitalWrite(LED_PIN, HIGH);
    pcmFill = 0; gSentMs = 0; gWaitSince = 0;
    gPeak = 0; gRawPeak = 0; gClips = 0; gGotAudio = false;
    gSumSq = 0; gNSamp = 0;
    gTurnAudio = 0; gHeard = ""; gSaid = "";
    gGainStart = gGain;
    // ⚠️ হাই-পাস ফিল্টার এখানে রিসেট করি না, আর RX DMA-ও খালি করি না।
    //    যে ফার্মওয়্যারটা ল্যাপটপের সাথে ঠিকঠাক চলছে সেটাও করে না —
    //    বুটে একবার সেট হয়ে চলতেই থাকে। রিসেট করলে টার্নের প্রথম
    //    স্যাম্পলটা DC ধাক্কা খেয়ে আকাশে উঠে যায় (peak 32768-এর কারণ)।
    if (!sendActivity(true)) { gTalking = false; digitalWrite(LED_PIN, LOW); return; }
    faceSetState(FACE_LISTENING);
    faceSetMsg(MSG_SHUNCHHI);
    Serial.println("[rec] shuru");
  }

  // ── চেপে রাখা আছে → অডিও পাঠাই ──
  if (gTalking) {
    size_t got = 0;
    bool lineOk = true;
    if (i2s_read(I2S_MIC, rawBuf, sizeof(rawBuf), &got, I2S_WAIT) == ESP_OK) {
      size_t n = got / sizeof(int32_t);
      // ⚠️ এক ফোঁটাও ফেলা যাবে না। বাফার ভরে গেলে সাথে সাথে পাঠিয়ে
      //    খালি করি, তারপর এই পড়াটার বাকি স্যাম্পলগুলো ঢুকতে থাকে।
      for (size_t i = 0; i < n && lineOk; i++) {
        pcmBuf[pcmFill++] = micSample(rawBuf[i]);
        if (pcmFill >= CHUNK_SAMPLES) lineOk = pushChunk();
      }
      // OLED-e mic-er level (chhoto janala, tai sosta)
      int32_t lv = gPeak / 47;                 // 11000 -> ~234
      faceMicLevel((uint8_t)(lv > 255 ? 255 : lv));
    }
    if (!lineOk) { gTalking = false; digitalWrite(LED_PIN, LOW); return; }

    if (!down) {
      pushChunk();                       // শেষ টুকরোটাও যাক
      sendActivity(false);
      gTalking = false;
      gWaitSince = millis();
      digitalWrite(LED_PIN, LOW);
      faceSetState(FACE_THINKING);
      faceSetMsg(MSG_BHABCHHI);

      // ── গড় আওয়াজ দেখে পরের বারের গেইন ঠিক করি ──
      // peak নয়, rms-ই আসল মাপ: কথার জন্য −২০ dBFS-এর কাছাকাছি
      // থাকলে Gemini সবচেয়ে ভালো বোঝে। একবারে ৪ গুণের বেশি
      // বদলাই না, নইলে দোল খেতে থাকবে।
      int32_t rms = turnRms();
      if (rms > 150) {
        int64_t ng = ((int64_t)gGain * AIM_RMS) / rms;
        if (ng > (int64_t)gGain * 4) ng = (int64_t)gGain * 4;
        if (ng < (int64_t)gGain / 4) ng = (int64_t)gGain / 4;
        if (ng < GAIN_MIN) ng = GAIN_MIN;
        if (ng > GAIN_MAX) ng = GAIN_MAX;
        gGain = (int32_t)ng;
      }

      Serial.printf("[rec] shesh — %u ms, gain %d, peak %d, rms %d\n",
                    (unsigned)gSentMs, (int)gGainStart, (int)gPeak, (int)rms);
      Serial.printf("[mic] rms %d  (chai ~%d).  kete gechhe %u ta sample\n",
                    (int)rms, AIM_RMS, (unsigned)gClips);
      if (gPeak < 500)
        Serial.println("[mic] PRAY NIRAB! mic-er tar / L-R pin dekhun.");
      if (gGain != gGainStart) {
        Serial.printf("[mic] gain %d -> %d kore dilam, porer bar theke ei tai\n",
                      (int)gGainStart, (int)gGain);
        saveGain();
      }
      Serial.println("[rec] uttor-er opekkha...");
    }
    return;
  }

  delay(2);
}
