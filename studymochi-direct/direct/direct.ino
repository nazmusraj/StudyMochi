// ════════════════════════════════════════════════════════════════
//   StudyMochi DIRECT — Standalone ESP32 Gemini Live client without a laptop.
//
//        ESP32 ──── WiFi ────► Gemini Live API (wss)
//          ▲                          │
//          └────── Audio Return ───────┘
//
//   ⚠️ This is an independent sketch. Existing studymochi_esp32 code remains untouched —
//      any changes here will not affect the original firmware.
//
//   ▸ Library: WiFiManager (tzapu) — only external dependency required
//     (WebSocket client is custom-built; see miniws.cpp)
//
//   ▸ On first boot, configuration AP launches: StudyMochi-Direct / mochi1234
//     Enter WiFi credentials and Gemini API key; saved to NVS.
//
//   ▸ Pinout wiring — identical to existing board setup:
//       INMP441 : SCK=33  WS=25  SD=32   VDD→3V3  L/R→GND
//       MAX98357A: BCLK=26 LRC=27 DIN=14  VIN→5V   GAIN→GND  SD→VIN
//       Hold BOOT button to talk
// ════════════════════════════════════════════════════════════════

#include <WiFi.h>
// ⚠️ Wire.h MUST be included here even though it is used in face.cpp.
//    The Arduino IDE resolves library dependencies by scanning the main .ino file.
//    If included only in face.cpp, the IDE fails to link the Wire library path,
//    resulting in "Wire.h: No such file or directory".
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
#include "imu.h"
#include "dfvoice.h"
#include "pomodoro_engine.h"
#include "stopwatch.h"
#include "mood.h"
#include "modes.h"


// ───────────────────── Configuration ─────────────────────
#define AP_NAME    "StudyMochi-Direct"
#define AP_PASS    "mochi1234"

#define MIC_SCK    33
#define MIC_WS     25
#define MIC_SD     32
#define AMP_BCLK   26
#define AMP_LRC    27
#define AMP_DIN    14
// ───── OLED ─────
// Same pins as laptop-connected firmware. Code executes normally even if OLED
// is disconnected — faceBegin() returns false, and all display routines
// safely exit as no-ops.
#define OLED_SDA   21
#define OLED_SCL   22
#define OLED_ADDR  0x3C

// ───── Touch Sensors (TTP223 x 2) ─────
// 3 wires per sensor: VCC->3V3 GND->GND OUT->GPIO specified below
//
//   Touch Normal (GPIO 18) : "Normal touch" dedicated to MODE NAVIGATION (Side sensor)
//                            - Long press (2s): Cycles main mode (Clock -> Timer -> AI -> Clock).
//                            - Single tap: Cycles sub-mode (Clock/Weather, Pomo/Timer/Stopwatch).
//                            - In AI mode: Tap OR 2s hold exits back to Clock mode.
//                            - NEVER triggers voice input.
//
//   Touch Voice (GPIO 19)  : "Other touch" dedicated to VOICE INPUT & PET (Head sensor)
//                            - In AI Mode: Hold to speak to Gemini Live API; release to listen.
//                            - In Clock Mode: Tap = pat (happy), Hold = cuddle, 3-tap = angry.
//                            - In Timer Mode: Tap = toggle start/pause, Hold = change profile / reset.
//
// If module is active LOW, configure TOUCH_ACTIVE_LOW 1 in touch.h.
#define TOUCH_NORMAL_PIN 18
#define TOUCH_VOICE_PIN  19

static int   gTouchNormalPin = TOUCH_NORMAL_PIN;
static int   gTouchVoicePin  = TOUCH_VOICE_PIN;

// ───── Pomodoro ─────
// ───── Timer ─────
#define TMR_STEP_MIN      5        // Minutes added per tap
#define TMR_MAX_MIN      60        // Reset threshold (exceeding rolls back to 0)
#define TMR_SET_WAIT_MS 3000       // Idle timeout before auto-start after setting

#define POMO_WORK_SEC  (25 * 60)
#define POMO_BREAK_SEC (5 * 60)

// ───── Weather Location (Dhaka default) ─────
// Configurable via setup portal
#define WX_LAT_DEFAULT 23.8103f
#define WX_LON_DEFAULT 90.4125f

#define BTN_PIN    0            // BOOT button — talk trigger (backup)
#define LED_PIN    2

// ───── Reset Button ─────
// One pin to GPIO 4, other pin to GND. Uses internal pull-up resistor;
// no external resistor required. Works safely even if unpopulated (floats HIGH).
//
// Holding 3 seconds clears WiFi + API key and reboots into setup portal.
// Blinking LED signals progress during hold.
#define RESET_PIN      4
#define RESET_HOLD_MS  3000

#define MIC_RATE   16000        // Required by Gemini Live API (16 kHz 16-bit mono PCM)
#define OUT_RATE   24000        // Gemini Live API return sample rate (24 kHz 16-bit mono PCM)
// Calibrated from recorded log measurements:
// logs/esp32-in-*.wav: At gain 16, RMS was -5.4 dBFS with
// 22.8% clipped samples. Speech target is RMS ≈ -20 dBFS.
// Attenuating by 15 dB implies ~5.4x lower gain: 16 / 5.4 ≈ 3.
#define MIC_GAIN   3
#define MIC_HPF_HZ 120

// Microphone input sensitivity dynamically self-calibrates:
//  - If any sample exceeds CLIP_AT, gain immediately decreases (clipping
//    severely impairs speech recognition)
//  - If overall turn amplitude is quiet, gain incrementally increases for next turn
//  - Calibrated gain is persisted in NVS across power cycles
#define CLIP_AT    20000        // -4 dBFS — clipping danger threshold
#define AIM_LOUD   11000        // Target ceiling upon clipping (-9 dBFS)
#define AIM_RMS    3000         // Target RMS at end of turn (-20 dBFS ideal for speech)
                                // (-20 dBFS — ideal for speech recognition)
#define GAIN_MIN   1
#define GAIN_MAX   4096

// Audio chunk size per transmit. 1600 samples = exactly 100 ms.
// Previously sent 256 samples (16 ms) — generating 62 messages/sec, each allocating
// a new String, causing heap fragmentation and socket disconnects.
#define CHUNK_SAMPLES  1600

#define GEM_HOST   "generativelanguage.googleapis.com"
#define GEM_PATH   "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent"
#define GEM_MODEL  "models/gemini-2.5-flash-native-audio-preview-12-2025"
#define GEM_VOICE  "Kore"

#define I2S_MIC    I2S_NUM_0
#define I2S_AMP    I2S_NUM_1
#define I2S_WAIT   pdMS_TO_TICKS(200)

// ───────────────────── State Variables ─────────────────────
static Preferences prefs;
static MiniWS ws;
static char    gApiKey[140] = "";
static bool    gReady   = false;      // Received setupComplete
static bool    gTalking = false;      // Currently recording
static bool    gMicOk = false, gAmpOk = false;
static uint32_t gPlayed = 0;
static uint32_t gSentMs = 0;          // Milliseconds of audio transmitted in this turn
static int32_t  gPeak   = 0;          // Peak microphone amplitude this turn (post-gain)
static int32_t  gRawPeak = 0;         // Raw microphone peak before gain
static uint32_t gClips  = 0;          // Count of clipped samples
static uint64_t gSumSq  = 0;          // Sum of squares for RMS calculation
static uint32_t gNSamp  = 0;
static uint32_t gWaitSince = 0;       // Timestamp when waiting started after activityEnd
static bool     gGotAudio = false;    // Whether response audio was received this turn
static uint32_t gTurnAudio = 0;       // Total audio bytes played back this turn
static uint32_t gTurnT0 = 0;          // Timestamp when response started streaming
static String   gHeard, gSaid;        // Turn transcripts buffered for atomic printing
static int32_t  gGain   = MIC_GAIN;   // Current gain setting (persisted in NVS)
static int32_t  gGainStart = MIC_GAIN; // Initial gain setting when this turn started

// ───── Touch, Clock, Weather, Pomodoro ─────
static Touch    tNormal, tVoice;
static float    gLat = WX_LAT_DEFAULT, gLon = WX_LON_DEFAULT;
static uint32_t gClockTick = 0;         // Refresh screen once per second
static bool     gPomoRun = false, gPomoBreak = false;
static int      gPomoLeft = POMO_WORK_SEC;
static int      gPomoRounds = 0;
static uint32_t gPomoTick = 0;
static bool     gSpeakOnIdle = false;   // Mochi speaks announcement once idle after pomodoro
static char     gSpeakWhat[160] = "";

// ───── Timer ─────
// Pomodoro is fixed at 25/5 minutes. Timer is customizable —
// duration adjusted by tapping Touch-2 on the timer screen.
static TimerMode gTmrMode = TM_IDLE;
static int       gTmrLeft = 0;          // Remaining seconds
static int       gTmrTotal = 0;         // Initial duration in seconds
static int       gTmrSetMin = 0;        // Current minutes in setting mode
static uint32_t  gTmrTick = 0;
static uint32_t  gTmrBlink = 0;         // Display flash state when timer expires
static bool      gTmrBlinkOn = true;
static int       gTmrBeeps = 0;         // Remaining alert beeps
static uint32_t  gTmrBeepAt = 0;

static int32_t rawBuf[256];
static int16_t pcmBuf[CHUNK_SAMPLES];
static size_t  pcmFill = 0;

// Audio message template formatted in-place — no String heap churn.
// prefix(70) + base64(4268) + suffix(4) + '\0'  ≈ 4343
// Official SDK sends data before mimeType — preserve this order
static const char AUD_PRE[]  = "{\"realtime_input\":{\"audio\":{\"data\":\"";
static const char AUD_POST[] = "\",\"mimeType\":\"audio/pcm;rate=16000\"}}}";
static char msgBuf[4608];

// High-pass filter (removes DC offset and 50Hz mains hum)
static float hpR = 0, hx1 = 0, hy1 = 0, hx2 = 0, hy2 = 0;
static inline float hpf(float v) {
  float o1 = v  - hx1 + hpR * hy1; hx1 = v;  hy1 = o1;
  float o2 = o1 - hx2 + hpR * hy2; hx2 = o1; hy2 = o2;
  return o2;
}

// Process one audio sample: Gain -> Limiter -> High-pass -> Clamp.
// The limiter is critical — clipping severely degrades recognition accuracy,
// so gain is reduced the moment a sample exceeds the safety threshold.
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
  gSumSq += (uint64_t)((int64_t)s * (int64_t)s);   // Accumulate for RMS
  gNSamp++;
  return (int16_t)s;
}

// Average amplitude over the turn
static int32_t turnRms() {
  if (!gNSamp) return 0;
  return (int32_t)sqrt((double)(gSumSq / gNSamp));
}

// ───── Forward Declarations (invoked by setup()) ─────
static void showHelp();
static void factoryReset(const char *why);
static void saveGain();
static void planRetry(const char *why, uint32_t ms);
static void turnReport();
static void beep(int hz, int ms, int amp);
static void tmrClear();                 // Needed before Serial 'k' handler

// ───── Exponential Reconnect Backoff ─────
// ⚠️ Exponential backoff prevents rapid reconnect loops that exhaust
// API rate limits on free-tier quotas when network connections drop.
// When connection fails, retry delay doubles up to maximum backoff ceiling.
//
static uint32_t gNextTry = 0;         // Next connection attempt timestamp
static uint32_t gBackoff = 0;         // Current backoff delay in milliseconds
static uint32_t gLastWaitMsg = 0;

// ───────────────────── I2S ─────────────────────
static void fillPins(i2s_pin_config_t &p, int bck, int wsp, int dout, int din) {
  memset(&p, 0xFF, sizeof(p));          // Initialize all pins to -1 including mck_io_num
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
  // ⚠️ 16 DMA buffers instead of 8:
  // Google streams 40 ms audio chunks, but network jitter varies packet arrival.
  // 8 buffers provide only ~85 ms buffering — a brief packet delay empties DMA,
  // resulting in audible audio stuttering and glitching.
  // 16 buffers store ~170 ms, comfortably absorbing network jitter.
  // Memory cost: 8 KB RAM (ample free memory available).
  c.dma_buf_count = 16; c.dma_buf_len = 256;
  c.use_apll = false; c.tx_desc_auto_clear = true;
  i2s_pin_config_t p; fillPins(p, AMP_BCLK, AMP_LRC, AMP_DIN, I2S_PIN_NO_CHANGE);
  if (i2s_driver_install(I2S_AMP, &c, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(I2S_AMP, &p) != ESP_OK) return false;
  i2s_zero_dma_buffer(I2S_AMP);
  return true;
}

// ── Playback Volume ──
// Gemini audio arrives near full digital scale (peak ~32323 / 32767).
// With GAIN pin tied to GND, MAX98357A adds +12 dB. Small speakers
// may distort or clip. Attenuate before I2S playback.
// Adjustable at runtime using the 'v' Serial command.
static int gVol = 70;                    // 0-100% volume scale

// The playback scaling loop touches every sample, allowing amplitude envelope
// measurement for lip-sync at zero additional CPU cost.
static uint8_t gEnv = 0;                 // Amplitude envelope (0..255)

static void applyVol(uint8_t *b, size_t n) {
  int16_t *s = (int16_t *)b;             // PCM buffer is 4-byte aligned
  size_t m = n / 2;
  int32_t peak = 0;
  for (size_t i = 0; i < m; i++) {
    int32_t v = s[i];
    if (gVol < 100) { v = (v * gVol) / 100; s[i] = (int16_t)v; }
    if (v < 0) v = -v;
    if (v > peak) peak = v;
  }
  // Speech peaks typically occupy ~half dynamic range; scale by 2 for lip-sync sensitivity
  int32_t e = peak * 2 / 129;            // 32767*2/129 ≈ 508 -> clamp
  if (e > 255) e = 255;
  // Fast attack, slow decay filter — prevents erratic mouth jittering
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
    if (w == 0 && millis() - t0 > 500) break;   // DMA buffer timeout — avoid hanging
  }
}

// ───────────────────── Live API: Transmission ─────────────────────
// ⚠️⚠️ Do NOT modify the case conventions of the JSON keys below.
//
// Live API schema requires an exact mixture of snake_case and camelCase:
// verified by capturing live payloads sent by the official google-genai SDK:
//
//   {"realtime_input":{"activityStart":{}}}          <- outer snake_case,
//   {"client_content":{"turns":[...],"turnComplete":true}}   inner camelCase
// Sub-fields under setup speechConfig / realtimeInputConfig
// also use snake_case: voice_config, voice_name, etc.
//   automatic_activity_detection, language_codes
//
// Sending pure camelCase ("realtimeInput") accepted the handshake but
// silently dropped audio chunks, causing Gemini to hear silence.
// Now matched byte-for-byte to the official SDK.
//
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

// Sends a test question without microphone — Serial command: t <question>
// Validates WiFi, Gemini Live API, and audio playback pipeline independently
// of microphone input hardware.
static bool sendTextTurn(const char *q) {
  String s = F("{\"client_content\":{\"turns\":[{\"parts\":[{\"text\":\"");
  for (const char *p = q; *p; p++) {          // Filter characters that could corrupt JSON payload
    if (*p == '"' || *p == '\\') continue;
    if ((unsigned char)*p < 0x20) continue;
    s += *p;
  }
  s += F("\"}],\"role\":\"user\"}],\"turnComplete\":true}}");
  Serial.printf("[api] proshno pathacchi: %s\n", q);
  return ws.sendText(s);
}

// Encodes 100 ms audio chunk to base64 directly inside msgBuf —
// avoids intermediate buffer allocations or String objects.
static bool sendAudioChunk(const int16_t *pcm, size_t samples) {
  const size_t pre  = sizeof(AUD_PRE)  - 1;
  const size_t post = sizeof(AUD_POST) - 1;
  size_t room = sizeof(msgBuf) - pre - post;      // Accommodates trailing null byte

  size_t outLen = 0;
  if (mbedtls_base64_encode((unsigned char *)msgBuf + pre, room, &outLen,
                            (const unsigned char *)pcm, samples * 2) != 0) {
    Serial.println("[api] base64 buffer chhoto — chunk baad");
    return false;
  }
  memcpy(msgBuf, AUD_PRE, pre);                   // Prepend JSON message prefix
  memcpy(msgBuf + pre + outLen, AUD_POST, post);  // Overwrite mbedtls trailing null with JSON suffix
  return ws.sendText(msgBuf, pre + outLen + post);
}

// Append brief silence flush — prevents audible DAC pop from lingering DMA samples
static void speakerSilence() {
  if (!gAmpOk) return;
  static const uint8_t z[512] = {0};
  for (int i = 0; i < 4; i++) speakerWrite(z, sizeof(z));
}

// Test beep — verifies I2S amplifier and speaker connectivity in 1 second
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
      float env = 1.0f;                       // Smooth envelope fade-in / fade-out
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

// Turn completion summary — printed atomically after playback completes
static void turnReport() {
  speakerSilence();
  float sec  = gTurnAudio / (2.0f * OUT_RATE);          // Spoken audio duration (seconds)
  float wall = (millis() - gTurnT0) / 1000.0f;          // Elapsed wall clock latency (seconds)
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

// ───────────────────── Live API: Reception ─────────────────────
// Stream-parses frames byte-by-byte. When matching "data":"...",
// decodes base64 in 4-byte chunks and pushes directly to I2S DMA.
static void handleServerFrame(uint64_t len) {
  (void)len;
  // Sliding window buffer for key pattern matching
  char win[24] = {0};
  int  wl = 0;

  bool inData = false;          // Currently inside audio payload ("data":"...")
  bool sawInline = false;       // Observed inlineData (audio stream, not transcript text)
  char q[4]; int qn = 0;        // Accumulates 4 base64 characters
  uint8_t pcm[768] __attribute__((aligned(4))); size_t pn = 0;
  uint32_t audioBytes = 0;
  String textOut;               // Transcript text snippet
  bool inText = false;
  bool inputTx = false;         // Distinguishes user input transcription vs Mochi response
  bool textIsInput = false;

  // ⚠️ Tolerates flexible whitespace: handles "data":" as well as "data" : "
  //    without desynchronizing the streaming parser.
  //
  //
  //
  //
  enum { W_NONE, W_DATA, W_TEXT } waitVal = W_NONE;
  int waitStage = 0;            // 0 = seeking ':', 1 = seeking opening quote '"'
  bool turnDone = false;

  // Save frame head for error diagnostics if server rejects request
  //
  char head[200]; size_t hn = 0;

  auto flush = [&]() { if (pn) { applyVol(pcm, pn);
                               speakerWrite(pcm, pn);
                               faceMouth(gEnv);        // ⭐ Lip-sync mouth animation
                               audioBytes += pn; pn = 0; } };

  while (true) {
    int c = ws.readByte();
    if (c < 0) break;

    if (inData) {
      if (c == '"') {                        // End of base64 audio payload
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
          // ⚠️ Do not print Serial output during playback: UART delays starve
          //    I2S DMA and cause audio glitching. Buffer and print after turn completes.
          //
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

    // ── Key matched; now seeking value token ──
    if (waitVal != W_NONE) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
      if (waitStage == 0) {
        if (c == ':') { waitStage = 1; continue; }
        waitVal = W_NONE;                     // False match — reset
      } else {
        if (c == '"') {
          if (waitVal == W_DATA) { inData = true; sawInline = false; qn = 0; }
          else { inText = true; textIsInput = inputTx; textOut = ""; }
          waitVal = W_NONE; wl = 0; win[0] = 0;
          continue;
        }
        waitVal = W_NONE;                     // Non-string token (e.g. null) — ignore
      }
    }

    // ── Shift Sliding Window ──
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
        turnDone = true;                 // Turn complete — print summaries after frame
      } else {
        // Turn concluded with no spoken audio (e.g. silence or unrecognized input)
        //
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

  // Server returned an error message — print for visibility
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
    // ⚠️ Avoid per-frame logging: 200+ lines at 115200 baud starves I2S DMA
    //    and causes stuttering.
    //
  }
  if (textOut.length()) {                 // Flush any partial transcript snippet
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
      // ── Parse closure status code and reason string ──
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

// ───────────────────── Session Establishment ─────────────────────
static bool openSession() {
  char path[400];
  snprintf(path, sizeof(path), "%s?key=%s", GEM_PATH, gApiKey);
  // SDK sends API key in header; URL query parameter provided as fallback
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

// ───────────────────── Button Initialization ─────────────────────
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

  // ── Touch Sensor Configuration ──
  prefs.begin("mochidirect", true);
  gTouchNormalPin = prefs.getInt("pin_norm", TOUCH_NORMAL_PIN);
  gTouchVoicePin  = prefs.getInt("pin_voic", TOUCH_VOICE_PIN);
  prefs.end();

  tNormal.begin(gTouchNormalPin, 2000);  // Normal touch: 2.0s hold for mode change
  tVoice.begin(gTouchVoicePin,   1500);  // Other touch: Voice input in AI mode / Pet interaction
  Serial.printf("[touch] Normal (Mode): GPIO %d | Other (Voice/Pet): GPIO %d\n",
                gTouchNormalPin, gTouchVoicePin);
  clockBegin();

  // MPU6050 (0x69 with AD0 pulled HIGH)
  bool imuOk = imuBegin(MPU6050_ADDR);
  Serial.printf("[imu] MPU6050 (0x%02X): %s\n", MPU6050_ADDR, imuOk ? "OK" : "Pai ni");

  // DFPlayer Mini Bangla Voice (HardwareSerial2: RX=16, TX=17, Vol=24)
  dfvoiceBegin(16, 17, 24);
  Serial.println("[dfplayer] Serial2 chalu (RX=16, TX=17)");

  gPomodoro.begin();
  gStopwatch.begin();
  gMood.begin();
  gModes.begin();


  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);      // Safe if unpopulated (floats HIGH)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  hpR = expf(-2.0f * PI * MIC_HPF_HZ / MIC_RATE);
  gMicOk = micBegin();
  gAmpOk = ampBegin();
  Serial.printf("[i2s] mic %s | amp %s\n", gMicOk ? "OK" : "BYARTHO",
                gAmpOk ? "OK" : "BYARTHO");

  // ── Load API Key from NVS ──
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

  // ── WiFi & API Key Configuration Portal ──
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

  // ── Portal Trigger Conditions ──
  // ⚠️ ESP32 preserves previous WiFi credentials in flash, which would cause
  //    autoConnect() to bypass portal setup and prevent entering an API key.
  //    Therefore, launch portal unconditionally if no API key is stored.
  bool needKey = strlen(gApiKey) < 10;

  // Also launches portal if reset button is held during boot
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

  // ── Save Configured Key to NVS ──
  if (strlen(pKey.getValue()) > 10) {
    strncpy(gApiKey, pKey.getValue(), sizeof(gApiKey) - 1);
    gApiKey[sizeof(gApiKey) - 1] = 0;
    prefs.begin("mochidirect", false);
    prefs.putString("key", gApiKey);
    prefs.end();
    Serial.println("[nvs] API key save holo");
  }
  // Weather location — persist if updated in portal
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

  // WiFi connected — sync time from NTP (Bangladesh UTC+6).
  Serial.println("[wifi] WiFi OK — NTP theke somoy anchhi...");
  clockSyncNTP(6 * 3600);
  MochiTime t = clockNow();
  Serial.printf("[rtc] ekhon %02d:%02d:%02d  %02d/%02d/%04d (valid=%d)\n",
                t.hour24, t.minute, t.second, t.day, t.month, t.year, t.valid);
  weatherFetch(gLat, gLon);        // Initial weather fetch
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
// ───── Serial Monitor Commands ─────
//   p  -> Launch portal (reconfigure WiFi & API key, preserving existing)
//   r  -> Factory reset (wipe WiFi + API key and restart)
//   i  -> Display current system status
// Wipes both WiFi credentials and API key, restarting in setup mode.
// Triggered by Serial 'r' command or 3-second GPIO 4 button hold.
static void factoryReset(const char *why) {
  Serial.printf("\n[reset] %s — SOB MUCHE DICCHI (WiFi + API key)...\n", why);
  ws.stop();
  prefs.begin("mochidirect", false);
  prefs.clear();                                 // Clear API key
  prefs.end();
  WiFiManager wm;
  wm.resetSettings();                            // Clear saved WiFi credentials
  Serial.printf("[reset] muche gechhe. phone diye '%s' hotspot-e jurun\n", AP_NAME);
  for (int i = 0; i < 6; i++) {                  // Signal visual confirmation via LED
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);  delay(80);
  }
  delay(400);
  ESP.restart();
}

static void showHelp() {
  Serial.println("\n  ── Serial command ──");
  Serial.println("   o + ENTER   : OLED SH1106 <-> SSD1306 bodlao");
  Serial.println("   w + ENTER   : Touch sensor pin swap (Normal <-> Voice)");
  Serial.println("  ── touch ──");
  Serial.printf ("   GPIO %d (Normal Touch) : 2s CHEPE DHORLE main mode bodlay\n", gTouchNormalPin);
  Serial.println("                             1 chap: sub-mode (AI mode-e Clock-e phere)");
  Serial.printf ("   GPIO %d (Other/Head)   : AI mode-e chepe dhorle kotha bola\n", gTouchVoicePin);
  Serial.println("                             Clock mode-e আদর / pet interaction");
  Serial.println("                             Timer mode-e start/pause/profile");
  Serial.println("   s + ENTER   : speaker beep — tar thik achhe ki");
  Serial.println("   v <0-100>   : speaker volume (blurry hole koman)");
  Serial.println("   t <proshno> : mic chhara likhe proshno korun");
  Serial.println("                 (mukhe uttor ele API+speaker thik)");
  Serial.println("   g <number>  : mic gain hate bodlan (g = ekhonkar man)");
  Serial.println("   c + ENTER : NTP theke notun kore somoy sync koro");
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

  // Read full input line to support arguments: t <question> or g <gain>
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

  if (c == 'w' || c == 'W') {
    int tmp = gTouchNormalPin;
    gTouchNormalPin = gTouchVoicePin;
    gTouchVoicePin  = tmp;
    prefs.begin("mochidirect", false);
    prefs.putInt("pin_norm", gTouchNormalPin);
    prefs.putInt("pin_voic", gTouchVoicePin);
    prefs.end();
    tNormal.begin(gTouchNormalPin, 2000);
    tVoice.begin(gTouchVoicePin,   1500);
    Serial.printf("[touch] SWAPPED! Normal (Mode): GPIO %d | Other (Voice/Pet): GPIO %d\n",
                  gTouchNormalPin, gTouchVoicePin);
    return;
  }

  if (c == 'c' || c == 'C') {
    Serial.println("[cmd] NTP theke notun kore somoy anchhi...");
    clockSyncNTP(6 * 3600);
    MochiTime t = clockNow();
    Serial.printf("[rtc] ekhonkar somoy: %02d:%02d:%02d  %02d/%02d/%04d\n",
                  t.hour24, t.minute, t.second, t.day, t.month, t.year);
    if (faceScreen() == SCR_CLOCK) {
      faceClockData(t.hour24, t.minute, t.second, t.day, t.month, t.year, t.dow, t.valid);
      faceRedraw();
    }
    return;
  }

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

// Checks if GPIO 4 reset button was held for 3 seconds — called from loop()
static void checkResetButton() {
  static uint32_t downSince = 0;
  static uint32_t lastBlink = 0;
  static bool     fired = false;      // Trigger once per press; require release before re-arming

  if (digitalRead(RESET_PIN) != LOW) {            // Button released
    if (downSince && !fired) {                    // Released before hold duration threshold
      Serial.println("[reset] batil kora holo");
      digitalWrite(LED_PIN, LOW);
    }
    downSince = 0;
    fired = false;                                // Re-arm for subsequent presses
    return;
  }

  if (fired) return;                              // Already fired — suppress repeat triggers

  uint32_t now = millis();
  if (!downSince) {
    downSince = now;
    Serial.printf("[reset] botam chepe achhe... %us dhore dhore rakhun\n",
                  RESET_HOLD_MS / 1000);
    return;
  }

  uint32_t held = now - downSince;

  // LED blink rate accelerates to indicate hold progress
  uint32_t period = held > 2000 ? 80 : (held > 1000 ? 160 : 300);
  if (now - lastBlink > period) {
    lastBlink = now;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if (held >= RESET_HOLD_MS) {
    fired = true;                                 // Mark fired to prevent re-triggering while held
    factoryReset("GPIO 4 botam");
  }
}

// Schedules next reconnect attempt and logs reason clearly
static void planRetry(const char *why, uint32_t ms) {
  if (ms < 15000)  ms = 15000;         // Minimum retry window: 15 seconds
  if (ms > 300000) ms = 300000;        // Maximum retry window: 5 minutes
  gBackoff = ms;
  gNextTry = millis() + ms;
  gLastWaitMsg = 0;
  faceSetState(FACE_WAITING);
  faceSetWait((int)(ms / 1000));   // Format countdown on display
  Serial.printf("[ws] %s — %u second por abar cheshta korbo\n",
                why, (unsigned)(ms / 1000));
}

static void saveGain() {
  prefs.begin("mochidirect", false);
  prefs.putInt("gain", gGain);
  prefs.end();
}

// Transmits an audio chunk. Disconnects if transmission fails.
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

// ───────────────────── Touch, Display & Pomodoro ─────────────────────
// Queues spoken announcement (e.g. pomodoro completion) to play
// only once Mochi returns to IDLE state to prevent interrupting speech.
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
      pomoStart(true);                       // Immediately start break phase
    } else {
      Serial.println("[pomo] biroti shesh");
      askMochi("Bishram shesh. Amake abar porte bosar janno ek line-e utsaho dao.");
      gPomoLeft = POMO_WORK_SEC;             // Next work round starts manually
    }
  }
  facePomoData(gPomoLeft, gPomoRun, gPomoBreak, gPomoRounds);
  if (faceScreen() == SCR_POMO) faceRedraw();
}

// ───────────────────── Custom Timer ─────────────────────
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
  // ── Alert Beeps ──
  // beep() is blocking, so play single beeps spaced 450 ms apart
  // allowing the main loop() to continue running smoothly.
  if (gTmrBeeps > 0 && (int32_t)(now - gTmrBeepAt) >= 0) {
    beep(880, 220, 9000);
    gTmrBeeps--;
    gTmrBeepAt = now + 450;
  }

  // ── Flash screen alert when completed ──
  if (gTmrMode == TM_DONE) {
    if (now - gTmrBlink >= 500) {
      gTmrBlink = now;
      gTmrBlinkOn = !gTmrBlinkOn;
      tmrPush();
    }
    return;
  }

  // ── Auto-start countdown after setting idle timeout ──
  // With a single button interface, releasing for 3 seconds
  // automatically commits the configured minutes and starts the countdown.
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
    gTmrBeeps   = 3;                      // 3 alert beeps (no network/quota required)
    gTmrBeepAt  = now;                    //
    gTmrBlink   = now;
    gTmrBlinkOn = true;
    Serial.printf("[timer] somoy shesh (%d minute)\n", gTmrTotal / 60);
  }
  tmrPush();
}

// Single tap on Timer screen — manages all timer states
static void tmrTap(uint32_t now) {
  switch (gTmrMode) {
    case TM_IDLE:
      gTmrMode   = TM_SET;
      gTmrSetMin = TMR_STEP_MIN;
      gTmrTick   = now;
      Serial.printf("[timer] bosachhi — %d minute\n", gTmrSetMin);
      break;

    case TM_SET:
      // 5 -> 10 -> ... -> 60 -> 0 (0 cancels timer)
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
      gTmrBeeps = 0;                      // Silence beeps
      tmrClear();
      Serial.println("[timer] muchhe dilam");
      return;
  }
  tmrPush();
}


// Redraw clock screen once per second
static void clockTick(uint32_t now) {
  if (faceScreen() != SCR_CLOCK) return;
  if (now - gClockTick < 1000) return;
  gClockTick = now;
  MochiTime t = clockNow();
  faceClockData(t.hour24, t.minute, t.second, t.day, t.month, t.year,
                t.dow, t.valid);
  faceRedraw();
}

void loop() {
  uint32_t now = millis();
  tNormal.update(now);
  tVoice.update(now);

  // ── Automatic NTP Sync whenever WiFi connects or reconnects ──
  static bool sWasWifiConnected = false;
  static uint32_t sLastNtpSync = 0;
  bool isWifiConnected = WiFi.isConnected();
  if (isWifiConnected) {
    if (!sWasWifiConnected) {
      Serial.println("[wifi] WiFi connect holo — NTP theke notun somoy anchhi...");
      if (clockSyncNTP(6 * 3600)) sLastNtpSync = now;
    } else if (now - sLastNtpSync >= 3600000) { // re-sync every 1 hour
      if (clockSyncNTP(6 * 3600)) sLastNtpSync = now;
    }
  }
  sWasWifiConnected = isWifiConnected;

  imuUpdate(now);
  gMood.tick(now);
  gPomodoro.tick(now);
  gStopwatch.tick(now);
  tmrTick(now);
  clockTick(now);
  gModes.update(now, tVoice, tNormal);

  // Sync Pomodoro data to face renderer
  facePomoDataExt(gPomodoro.remainingSec(), gPomodoro.totalSec(), gPomodoro.isRunning(),
                  (int)gPomodoro.phase(), gPomodoro.currentRound(),
                  gPomodoro.currentProfile().label);

  // Sync Stopwatch data to face renderer
  faceStopwatchData(gStopwatch.elapsedMs(), gStopwatch.isRunning());

  // Check for Pomodoro phase transitions
  PomoPhase newPomoPhase;
  if (gPomodoro.tookPhaseChange(newPomoPhase)) {
    if (newPomoPhase == POMO_PHASE_WORK) {
      dfvoicePlay(VOICE_POMO_START);
      faceRedraw();
    } else if (newPomoPhase == POMO_PHASE_BREAK || newPomoPhase == POMO_PHASE_LONG_BREAK) {
      dfvoicePlay(VOICE_POMO_BREAK);
      gMood.triggerEvent(MOOD_EVT_POMO_COMPLETE);
      faceRedraw();
    }
  }

  checkSerialCmd();
  checkResetButton();
  faceTick();                      // chokher polok, bhabnar bindu
  if (strlen(gApiKey) < 10) { delay(1000); return; }

  // Reconnect dropped WebSocket session using exponential backoff
  if (!ws.connected()) {
    gTalking = false; pcmFill = 0; digitalWrite(LED_PIN, LOW);

    int32_t left = (int32_t)(gNextTry - millis());
    if (left > 0) {                       // Backoff delay has not elapsed yet
      if (millis() - gLastWaitMsg > 30000) {
        gLastWaitMsg = millis();
        Serial.printf("[ws] opekkha... aro %d second\n", left / 1000);
      }
      delay(50);
      return;                             // Non-blocking: loop() handles button & Serial
    }

    Serial.println("[ws] session khulchi...");
    faceSetMsg(MSG_JUKTECHHI);
    if (!openSession()) {
      planRetry("khola gelo na", gBackoff ? gBackoff * 2 : 15000);
      return;
    }
    gBackoff = 0;                         // Reset backoff upon successful connection
    Serial.println(">>> BOOT BOTAM CHEPE DHORE KOTHA BOLUN <<<");
  }

  pumpWs();
  if (!ws.connected()) return;          // Connection may have been severed by pumpWs

  // Check for response latency timeout
  if (gWaitSince && millis() - gWaitSince > 30000) {
    gWaitSince = 0;
    Serial.println("[api] 30s dhore kono uttor elo na.");
    Serial.println("      mic peak dekhun — 500-er niche hole mic-e sound jacche na.");
  }

  // Play queued Pomodoro announcement once Mochi is idle
  if (gSpeakOnIdle && gReady && !gTalking && !gWaitSince) {
    gSpeakOnIdle = false;
    gGotAudio = false;
    faceSetState(FACE_THINKING);
    if (sendTextTurn(gSpeakWhat)) gWaitSince = millis();
  }

  // Talk button: BOOT button always talks; other touch sensor (tVoice) talks ONLY in AI mode.
  // The normal touch sensor (tNormal) is STRICTLY reserved for mode switching and NEVER triggers voice input.
  bool down = btnDown() || (gModes.isAiMode() && tVoice.isDown());


  // ── Talk Button Pressed ──
  if (down && !gTalking && gReady) {
    gTalking = true;
    digitalWrite(LED_PIN, HIGH);
    pcmFill = 0; gSentMs = 0; gWaitSince = 0;
    gPeak = 0; gRawPeak = 0; gClips = 0; gGotAudio = false;
    gSumSq = 0; gNSamp = 0;
    gTurnAudio = 0; gHeard = ""; gSaid = "";
    gGainStart = gGain;
    // ⚠️ Do not reset high-pass filter or purge RX DMA here.
    //    Preserves continuous DC bias tracking across speech turns.
    //    Resetting injects a transient DC spike that can cause initial clipping.
    //
    if (!sendActivity(true)) { gTalking = false; digitalWrite(LED_PIN, LOW); return; }
    faceSetState(FACE_LISTENING);
    faceSetMsg(MSG_SHUNCHHI);
    Serial.println("[rec] shuru");
  }

  // ── Talk Button Held -> Stream Audio ──
  if (gTalking) {
    // If user touches normal mode change sensor (tNormal) while talking, abort speech immediately
    // so mode change is fast, responsive, and never blocked by AI mode!
    if (tNormal.tookTap() || tNormal.tookHold()) {
      sendActivity(false);
      gTalking = false;
      digitalWrite(LED_PIN, LOW);
      gModes.nextMainMode();
      return;
    }
    size_t got = 0;
    bool lineOk = true;
    if (i2s_read(I2S_MIC, rawBuf, sizeof(rawBuf), &got, I2S_WAIT) == ESP_OK) {
      size_t n = got / sizeof(int32_t);
      // ⚠️ Zero sample loss: Flush filled chunks immediately and continue buffering
      //    remaining samples seamlessly.
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
      pushChunk();                       // Transmit final partial chunk
      sendActivity(false);
      gTalking = false;
      gWaitSince = millis();
      digitalWrite(LED_PIN, LOW);
      faceSetState(FACE_THINKING);
      faceSetMsg(MSG_BHABCHHI);

      // ── Automatic Gain Control: Adjust gain for next turn based on RMS ──
      // Target RMS near -20 dBFS for optimal speech recognition.
      // Step size limited to prevent oscillating gain feedback.
      //
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
