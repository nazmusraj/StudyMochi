// StudyMochi firmware for a Bengali desk companion on a classic ESP32.
// It combines clock/weather pages, three timer tools, pet interactions,
// MPU6050 orientation gestures, SD announcements, and Gemini Live audio.
// The complete pin map and operating instructions are in the root README.

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
#include "config.h"
#include "audio_manager.h"
#include "pomodoro_engine.h"
#include "stopwatch.h"
#include "mood.h"
#include "modes.h"


// ───────────────────── Configuration ─────────────────────
#define AP_NAME MOCHI_AP_NAME
#define AP_PASS MOCHI_AP_PASS
#define MIC_SCK PIN_MIC_BCLK
#define MIC_WS PIN_MIC_LRCLK
#define MIC_SD PIN_MIC_DATA
#define OLED_SDA PIN_OLED_SDA
#define OLED_SCL PIN_OLED_SCL
#define OLED_ADDR OLED_I2C_ADDR
#define TOUCH_NORMAL_PIN PIN_TOUCH_SIDE
#define TOUCH_VOICE_PIN PIN_TOUCH_HEAD

static int   gTouchNormalPin = TOUCH_NORMAL_PIN;
static int   gTouchVoicePin  = TOUCH_VOICE_PIN;

// ───── Timer ─────
#define TMR_STEP_MIN      5        // Minutes added per tap
#define TMR_MAX_MIN      60        // Reset threshold (exceeding rolls back to 0)
#define TMR_SET_WAIT_MS 3000

// ───── Weather Location (Dhaka default) ─────
// Configurable via setup portal
#define WX_LAT_DEFAULT DEFAULT_LATITUDE
#define WX_LON_DEFAULT DEFAULT_LONGITUDE

#define BTN_PIN PIN_BOOT_BUTTON
#define LED_PIN PIN_STATUS_LED

// ───── Reset Button ─────
// One pin to GPIO 4, other pin to GND. Uses internal pull-up resistor;
// no external resistor required. Works safely even if unpopulated (floats HIGH).
//
// Holding 3 seconds clears WiFi + API key and reboots into setup portal.
// Blinking LED signals progress during hold.
#define RESET_PIN PIN_FACTORY_RESET
#define RESET_HOLD_MS  3000

#define MIC_RATE MIC_SAMPLE_RATE
#define OUT_RATE SPEAKER_SAMPLE_RATE
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
static long     gStoredUtcOffset = 6 * 3600;
static uint32_t gClockTick = 0;         // Refresh screen once per second

// ───── Timer ─────
// The normal timer is adjustable in five-minute steps.
static TimerMode gTmrMode = TM_IDLE;
static int       gTmrLeft = 0;          // Remaining seconds
static int       gTmrTotal = 0;         // Initial duration in seconds
static int       gTmrSetMin = 0;        // Current minutes in setting mode
static uint32_t  gTmrTick = 0;
static uint32_t  gTmrBlink = 0;         // Display flash state when timer expires
static bool      gTmrBlinkOn = true;
static bool      gTmrAlerted = false;

// A pet animation temporarily owns the display, then returns to the exact
// clock/weather/timer page that was active before the interaction.
static bool      gPetOverlay = false;
static uint32_t  gPetOverlayUntil = 0;
static bool      gAiListening = false;
static uint32_t  gAiStartAfter = 0;
static uint32_t  gLastUiRefresh = 0;
static bool      gFaceDownDnd = false;
static bool      gPomoResumeSilently = false;

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
  return (int32_t)sqrt((double)gSumSq / (double)gNSamp);
}

// ───── Forward Declarations (invoked by setup()) ─────
static void showHelp();
static void factoryReset(const char *why);
static void saveGain();
static void planRetry(const char *why, uint32_t ms);
static void turnReport();
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

static int gVol = 70;                    // 0-100% volume scale
static uint8_t gEnv = 0;                 // Amplitude envelope (0..255)

static void speakerWrite(uint8_t *data, size_t bytes) {
  if (!gAmpOk) return;
  uint8_t instantEnvelope = 0;
  gAudio.writeAiPcm(data, bytes, (uint8_t)gVol, &instantEnvelope);
  gEnv = instantEnvelope > gEnv ? instantEnvelope :
         (uint8_t)((gEnv * 3 + instantEnvelope) / 4);
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
  gAudio.endAiStream();
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

  auto flush = [&]() { if (pn) { speakerWrite(pcm, pn);
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

  // The selected OLED controller type is persisted in NVS.
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
  // Restore devices that briefly saved GPIO 34 for the head sensor.
  if (gTouchVoicePin == 34) {
    gTouchVoicePin = TOUCH_VOICE_PIN;
    prefs.begin("mochidirect", false);
    prefs.putInt("pin_voic", gTouchVoicePin);
    prefs.end();
  }
  tNormal.begin(gTouchNormalPin, TOUCH_HOLD_MS);
  tVoice.begin(gTouchVoicePin, TOUCH_HOLD_MS);
  Serial.printf("[touch] Normal (Mode): GPIO %d | Other (Voice/Pet): GPIO %d\n",
                gTouchNormalPin, gTouchVoicePin);
  // MPU6050 (0x69 with AD0 pulled HIGH)
  bool imuOk = imuBegin(MPU6050_ADDR);
  Serial.printf("[imu] MPU6050 (0x%02X): %s\n", MPU6050_ADDR, imuOk ? "OK" : "Pai ni");

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
  gAmpOk = gAudio.begin(I2S_NUM_1);
  Serial.printf("[i2s] microphone %s | amplifier %s | microSD %s\n",
                gMicOk ? "ready" : "failed",
                gAmpOk ? "ready" : "failed",
                gAudio.sdReady() ? "ready" : "not found");

  // ── Load API Key from NVS ──
  prefs.begin("mochidirect", true);
  String k = prefs.getString("key", "");
  gGain = prefs.getInt("gain", MIC_GAIN);
  gVol  = prefs.getInt("vol", 70);
  gLat  = prefs.getFloat("lat", WX_LAT_DEFAULT);
  gLon  = prefs.getFloat("lon", WX_LON_DEFAULT);
  gStoredUtcOffset = prefs.getLong("utc_off", 6 * 3600);
  prefs.end();
  if (gGain < GAIN_MIN || gGain > GAIN_MAX) gGain = MIC_GAIN;
  if (gVol < 0 || gVol > 100) gVol = 70;
  gGainStart = gGain;
  strncpy(gApiKey, k.c_str(), sizeof(gApiKey) - 1);
  clockBegin(gStoredUtcOffset);
  Serial.printf("[mic] gain %d (nije nije thik hoye jabe)\n", (int)gGain);

  // ── WiFi & API Key Configuration Portal ──
  WiFiManager wm;
  WiFiManagerParameter pKey("key", "জেমিনি এপিআই কী", gApiKey,
                            sizeof(gApiKey) - 1);
  char latBuf[16], lonBuf[16];
  snprintf(latBuf, sizeof(latBuf), "%.4f", gLat);
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", gLon);
  WiFiManagerParameter pLat("lat", "অক্ষাংশ (ঢাকা ২৩.৮১০৩)",
                            latBuf, sizeof(latBuf) - 1);
  WiFiManagerParameter pLon("lon", "দ্রাঘিমাংশ (ঢাকা ৯০.৪১২৫)",
                            lonBuf, sizeof(lonBuf) - 1);
  wm.addParameter(&pKey);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.setConfigPortalTimeout(240);
  wm.setDarkMode(true);
  wm.setTitle("স্টাডিমোচি সেটআপ");

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

  // Start with the last known location offset. Open-Meteo may replace it
  // with the selected location's current offset below.
  clockConfigureNtp(gStoredUtcOffset);
  MochiTime t = clockNow();
  Serial.printf("[rtc] ekhon %02d:%02d:%02d  %02d/%02d/%04d (valid=%d)\n",
                t.hour24, t.minute, t.second, t.day, t.month, t.year, t.valid);
  weatherFetch(gLat, gLon, true);
  WeatherNow initialWeather = weatherGet();
  if (initialWeather.valid) {
    clockConfigureNtp(initialWeather.utcOffsetSeconds);
    prefs.begin("mochidirect", false);
    prefs.putLong("utc_off", initialWeather.utcOffsetSeconds);
    prefs.end();
    gStoredUtcOffset = initialWeather.utcOffsetSeconds;
    faceWeatherData(true, initialWeather.temperatureC,
                    initialWeather.apparentC, initialWeather.humidity,
                    initialWeather.weatherCode, initialWeather.windKmh,
                    initialWeather.isDay, initialWeather.maximumC,
                    initialWeather.minimumC, initialWeather.rainProbability);
  }
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
    tNormal.begin(gTouchNormalPin, TOUCH_HOLD_MS);
    tVoice.begin(gTouchVoicePin, TOUCH_HOLD_MS);
    Serial.printf("[touch] SWAPPED! Normal (Mode): GPIO %d | Other (Voice/Pet): GPIO %d\n",
                  gTouchNormalPin, gTouchVoicePin);
    return;
  }

  if (c == 'c' || c == 'C') {
    Serial.println("[cmd] NTP theke notun kore somoy anchhi...");
    clockConfigureNtp(clockUtcOffset());
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
    Serial.println("[test] playing the speaker test tone");
    gAudio.playSound(SOUND_HAPPY, AUDIO_PRIORITY_ALARM);
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
    Serial.printf("  pomodoro : phase %d (%u:%02u remaining, round %u)\n",
                  (int)gPomodoro.phase(),
                  (unsigned)(gPomodoro.remainingSec() / 60),
                  (unsigned)(gPomodoro.remainingSec() % 60),
                  (unsigned)gPomodoro.currentRound());
    showHelp();
    return;
  }

  if (c == 'k' || c == 'K') {
    gAudio.stop();
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
    WiFiManagerParameter pKey("key", "জেমিনি এপিআই কী", gApiKey,
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

// ───────────────────── Custom Timer ─────────────────────
static void tmrPush() {
  faceTimerData(gTmrLeft, gTmrTotal, gTmrSetMin, gTmrMode, gTmrBlinkOn);
  if (faceScreen() == SCR_TIMER) faceRedraw();
}

static void tmrStart(int minutes) {
  if (minutes < 1) minutes = 1;
  if (minutes > 180) minutes = 180;
  gTmrTotal = minutes * 60;
  gTmrLeft  = gTmrTotal;
  gTmrMode  = TM_RUN;
  gTmrTick  = millis();
  gTmrAlerted = false;
  gTmrBlinkOn = true;
  tmrPush();
  Serial.printf("[timer] %d minute chalu\n", minutes);
}

static void tmrClear() {
  gTmrMode = TM_IDLE;
  gTmrLeft = gTmrTotal = gTmrSetMin = 0;
  gTmrAlerted = false;
  gTmrBlinkOn = true;
  tmrPush();
}

static void tmrTick(uint32_t now) {
  // ── Flash screen alert when completed ──
  if (gTmrMode == TM_DONE) {
    if (now - gTmrBlink >= 500) {
      gTmrBlink = now;
      gTmrBlinkOn = !gTmrBlinkOn;
      tmrPush();
    }
    return;
  }

  if (gTmrMode == TM_SET) return;

  if (gTmrMode != TM_RUN) return;
  if (now - gTmrTick < 1000) return;
  gTmrTick += 1000;
  if (gTmrLeft > 0) gTmrLeft--;

  if (gTmrLeft == 0) {
    gTmrMode    = TM_DONE;
    if (!gTmrAlerted) {
      gAudio.playSound(SOUND_TIMER_DONE, AUDIO_PRIORITY_ALARM);
      gTmrAlerted = true;
    }
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
    case TM_PAUSE:
      return;

    case TM_DONE:
      gAudio.stop();
      tmrClear();
      Serial.println("[timer] muchhe dilam");
      return;
  }
  tmrPush();
}

static void tmrHold(uint32_t now) {
  switch (gTmrMode) {
    case TM_IDLE:
      gTmrSetMin = TMR_STEP_MIN;
      tmrStart(gTmrSetMin);
      return;
    case TM_SET:
      tmrStart(gTmrSetMin > 0 ? gTmrSetMin : TMR_STEP_MIN);
      return;
    case TM_RUN:
      gTmrMode = TM_PAUSE;
      break;
    case TM_PAUSE:
      gTmrMode = TM_RUN;
      gTmrTick = now;
      break;
    case TM_DONE:
      gAudio.stop();
      tmrClear();
      return;
  }
  tmrPush();
}


// Redraw clock screen once per second
static void clockTick(uint32_t now) {
  if (now - gClockTick < 1000) return;
  gClockTick = now;
  MochiTime t = clockNow();
  faceClockData(t.hour24, t.minute, t.second, t.day, t.month, t.year,
                t.dow, t.valid);
  if (faceScreen() == SCR_CLOCK) faceRedraw();
}

static void syncWeatherDisplay() {
  WeatherNow weather = weatherGet();
  faceWeatherData(weather.valid, weather.temperatureC, weather.apparentC,
                  weather.humidity, weather.weatherCode, weather.windKmh,
                  weather.isDay, weather.maximumC, weather.minimumC,
                  weather.rainProbability);
}

static bool showPetReaction(MoodEvent event, FaceState face, PetSound sound,
                            uint32_t durationMs) {
  if (gFaceDownDnd) return false;
  gMood.triggerEvent(event);
  gPetOverlay = true;
  gPetOverlayUntil = millis() + durationMs;
  faceSetScreen(SCR_FACE);
  faceSetState(face);
  gAudio.playSound(sound, AUDIO_PRIORITY_PET);
  return true;
}

static void finishPetOverlay(uint32_t now) {
  if (!gPetOverlay || (int32_t)(now - gPetOverlayUntil) < 0) return;
  gPetOverlay = false;
  faceSetState(gMood.currentFace());
  gModes.refreshScreen();
}

static void playModeAnnouncement(MainMode mode) {
  const char *path = mode == MODE_CLOCK ? AUDIO_CLOCK_MODE :
                     mode == MODE_TIMER ? AUDIO_TIMER_MODE : AUDIO_AI_MODE;
  if (!gAudio.playWav(path, AUDIO_PRIORITY_MODE)) {
    gAudio.playSound(SOUND_HAPPY, AUDIO_PRIORITY_MODE);
  }
}

static void stopAiInput() {
  gAiListening = false;
  if (!gTalking) return;
  pushChunk();
  sendActivity(false);
  gTalking = false;
  gWaitSince = millis();
  digitalWrite(LED_PIN, LOW);
  faceSetState(FACE_THINKING);
  faceSetMsg(MSG_BHABCHHI);
}

static void handleTouchEvents(uint32_t now) {
  bool sideHold = tNormal.tookHold();
  bool sideTap = tNormal.tookTap();
  bool topHold = tVoice.tookHold();
  bool topTap = tVoice.tookTap();
  bool rapidTap = false;
  if (gModes.currentMainMode() == MODE_CLOCK) {
    rapidTap = tVoice.tookRapidTap(PET_RAPID_TAP_COUNT,
                                   PET_RAPID_TAP_WINDOW_MS);
  }

  if (sideHold) {
    stopAiInput();
    gAudio.stop();
    gPetOverlay = false;
    gModes.nextMainMode();
    faceSetState(gMood.currentFace());
    playModeAnnouncement(gModes.currentMainMode());
    return;
  }

  if (sideTap) {
    if (gModes.isAiMode()) {
      stopAiInput();
      gAudio.stop();
      faceSetState(gMood.currentFace());
      faceSetMsg(MSG_BOLUN);
    } else {
      gPetOverlay = false;
      gModes.nextSubMode();
    }
    return;
  }

  if (gModes.currentMainMode() == MODE_CLOCK) {
    if (rapidTap) {
      // The third release is also a tap; suppress the happy reaction.
      (void)topTap;
      showPetReaction(MOOD_EVT_RAPID_TAP, FACE_ANGRY, SOUND_ANGRY, 3000);
    } else if (topHold) {
      showPetReaction(MOOD_EVT_CUDDLE, FACE_CUDDLE, SOUND_CUDDLE, 3500);
    } else if (topTap) {
      showPetReaction(MOOD_EVT_PAT, FACE_HAPPY, SOUND_HAPPY, 2200);
    }
    return;
  }

  if (gModes.currentMainMode() == MODE_TIMER) {
    uint8_t subMode = gModes.currentSubMode();
    if (subMode == SUB_TIMER_POMO) {
      if (topHold) {
        if (gPomodoro.isRunning()) gPomodoro.pause();
        else {
          gPomoResumeSilently = gPomodoro.phase() == POMO_PHASE_PAUSED;
          gPomodoro.start();
        }
        faceRedraw();
      } else if (topTap) {
        if (gPomodoro.phase() == POMO_PHASE_IDLE) {
          gPomodoro.nextProfile();
          facePomoDataExt(gPomodoro.remainingSec(), gPomodoro.totalSec(), false,
                          (int)gPomodoro.displayPhase(), gPomodoro.currentRound(),
                          gPomodoro.currentProfile().label);
          faceRedraw();
        } else {
          showPetReaction(MOOD_EVT_PAT, FACE_HAPPY, SOUND_HAPPY, 1600);
        }
      }
    } else if (subMode == SUB_TIMER_CUSTOM) {
      if (topHold) tmrHold(now);
      else if (topTap) {
        if (gTmrMode == TM_IDLE || gTmrMode == TM_SET || gTmrMode == TM_DONE)
          tmrTap(now);
        else
          showPetReaction(MOOD_EVT_PAT, FACE_HAPPY, SOUND_HAPPY, 1600);
      }
    } else {
      if (topHold) {
        gStopwatch.reset();
        faceStopwatchData(gStopwatch.elapsedMs(), false);
        faceRedraw();
      } else if (topTap) {
        gStopwatch.toggle();
      }
    }
    return;
  }

  if ((topTap || topHold) && gModes.isAiMode()) {
    if (!gReady || !ws.connected()) {
      gAudio.playSound(SOUND_INVALID, AUDIO_PRIORITY_PET);
      return;
    }
    if (gAiListening || gTalking) {
      gAudio.playSound(SOUND_LISTEN_OFF, AUDIO_PRIORITY_PET);
      stopAiInput();
    } else {
      gAudio.stop();
      gAiListening = true;
      gAiStartAfter = now + 160;
      gAudio.playSound(SOUND_LISTEN_ON, AUDIO_PRIORITY_PET);
    }
  }
}

static void handleImuEvents() {
  OrientFace orientation;
  if (imuTookOrientationChange(orientation)) {
    bool nowFaceDown = orientation == ORIENT_UPSIDE_DOWN;
    if (nowFaceDown != gFaceDownDnd) {
      gFaceDownDnd = nowFaceDown;
      if (gFaceDownDnd) {
        stopAiInput();
        gAudio.stop();
        faceSetDisplayEnabled(false);
      } else {
        faceSetDisplayEnabled(true);
        gModes.refreshScreen();
      }
    }

    if (!gFaceDownDnd && gModes.currentMainMode() == MODE_TIMER &&
        gModes.currentSubMode() == SUB_TIMER_POMO &&
        gPomodoro.phase() == POMO_PHASE_IDLE &&
        (orientation == ORIENT_UPRIGHT || orientation == ORIENT_TILT_RIGHT ||
         orientation == ORIENT_TILT_BACK || orientation == ORIENT_TILT_LEFT)) {
      gPomodoro.selectProfile(imuGetPomoPresetIndex());
      facePomoDataExt(gPomodoro.remainingSec(), gPomodoro.totalSec(), false,
                      (int)gPomodoro.displayPhase(), gPomodoro.currentRound(),
                      gPomodoro.currentProfile().label);
      faceRedraw();
    }

    if (!gFaceDownDnd &&
        (orientation == ORIENT_TILT_LEFT || orientation == ORIENT_TILT_RIGHT)) {
      gMood.triggerEvent(MOOD_EVT_TILT_SQUISH);
      gAudio.playSound(SOUND_SQUISH, AUDIO_PRIORITY_PET);
    }
  }

  if (!gFaceDownDnd && imuTookShake()) {
    showPetReaction(MOOD_EVT_SHAKE, FACE_DIZZY, SOUND_DIZZY, 3000);
  }

  if (!gFaceDownDnd && (gPetOverlay || gModes.isAiMode())) {
    faceSetSquish(imuSquishOffsetX(), imuSquishOffsetY());
  } else {
    faceSetSquish(0, 0);
  }
}

void loop() {
  uint32_t now = millis();
  tNormal.update(now);
  tVoice.update(now);

  bool isWifiConnected = WiFi.isConnected();
  gAudio.tick();
  clockUpdate(now, isWifiConnected);

  if (isWifiConnected && weatherNeedsRefresh(now) && weatherFetch(gLat, gLon)) {
    WeatherNow fresh = weatherGet();
    syncWeatherDisplay();
    if (faceScreen() == SCR_WEATHER_NOW ||
        faceScreen() == SCR_WEATHER_DETAILS ||
        faceScreen() == SCR_WEATHER_TODAY) {
      faceRedraw();
    }
    if (fresh.utcOffsetSeconds != clockUtcOffset()) {
      clockConfigureNtp(fresh.utcOffsetSeconds);
      prefs.begin("mochidirect", false);
      prefs.putLong("utc_off", fresh.utcOffsetSeconds);
      prefs.end();
      gStoredUtcOffset = fresh.utcOffsetSeconds;
    }
  }

  imuUpdate(now);
  gMood.tick(now);
  gPomodoro.tick(now);
  gStopwatch.tick(now);
  tmrTick(now);
  clockTick(now);
  handleTouchEvents(now);
  handleImuEvents();
  finishPetOverlay(now);

  facePomoDataExt(gPomodoro.remainingSec(), gPomodoro.totalSec(), gPomodoro.isRunning(),
                  (int)gPomodoro.displayPhase(), gPomodoro.currentRound(),
                  gPomodoro.currentProfile().label);
  faceStopwatchData(gStopwatch.elapsedMs(), gStopwatch.isRunning());
  if (now - gLastUiRefresh >= 100) {
    gLastUiRefresh = now;
    if (faceScreen() == SCR_STOPWATCH || faceScreen() == SCR_POMO) faceRedraw();
  }

  PomoPhase newPomoPhase;
  if (gPomodoro.tookPhaseChange(newPomoPhase)) {
    if (gPomoResumeSilently) {
      gPomoResumeSilently = false;
    } else if (newPomoPhase == POMO_PHASE_WORK) {
      if (!gAudio.playWav(AUDIO_POMO_START, AUDIO_PRIORITY_MODE)) {
        gAudio.playSound(SOUND_HAPPY, AUDIO_PRIORITY_MODE);
      }
    } else if (newPomoPhase == POMO_PHASE_BREAK) {
      if (!gAudio.playWav(AUDIO_BREAK_START, AUDIO_PRIORITY_ALARM)) {
        gAudio.playSound(SOUND_TIMER_DONE, AUDIO_PRIORITY_ALARM);
      }
      gMood.triggerEvent(MOOD_EVT_POMO_COMPLETE);
    } else if (newPomoPhase == POMO_PHASE_LONG_BREAK) {
      if (!gAudio.playWav(AUDIO_SESSION_DONE, AUDIO_PRIORITY_ALARM)) {
        gAudio.playSound(SOUND_TIMER_DONE, AUDIO_PRIORITY_ALARM);
      }
      gMood.triggerEvent(MOOD_EVT_POMO_COMPLETE);
    }
    faceRedraw();
  }

  checkSerialCmd();
  checkResetButton();
  faceTick();
  if (strlen(gApiKey) < 10) { delay(2); return; }

  // Reconnect dropped WebSocket session using exponential backoff
  if (!ws.connected()) {
    gTalking = false; pcmFill = 0; digitalWrite(LED_PIN, LOW);

    int32_t left = (int32_t)(gNextTry - millis());
    if (left > 0) {                       // Backoff delay has not elapsed yet
      if (millis() - gLastWaitMsg > 30000) {
        gLastWaitMsg = millis();
        Serial.printf("[ws] opekkha... aro %d second\n", left / 1000);
      }
      delay(2);
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

  // The BOOT button remains a hold-to-talk backup. The head sensor toggles
  // listening with a short touch in AI mode.
  bool down = btnDown() ||
              (gModes.isAiMode() && gAiListening &&
               (int32_t)(millis() - gAiStartAfter) >= 0);


  // ── Talk Button Pressed ──
  if (down && !gTalking && gReady) {
    gAudio.stop();
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
      // The microphone meter updates only a small OLED window.
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
