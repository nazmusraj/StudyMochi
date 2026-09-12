// ════════════════════════════════════════════════════════════════
//   Mochi Face & Display — SSD1306 OLED with synchronized speech lip-sync.
//
//   Why we do not use Adafruit_SSD1306:
//   ─────────────────────────────────────
//   Adafruit's display() flushes the entire 1024-byte framebuffer over I2C.
//   At 400 kHz, that takes ≈ 23 ms. Calling it 20 times a second for mouth
//   animation consumes 460 ms — meaning almost half of the ESP32's CPU time
//   is spent transmitting pixels. During that time, I2S DMA underruns,
//   causing audio playback stuttering and dropped samples.
//
//   Instead, we implemented a custom minimal driver. Using SSD1306 window
//   addressing, we transmit ONLY the mouth region (41 columns × 2 pages =
//   82 bytes ≈ 2 ms). At 20 fps, this takes only 40 ms per second.
//   Additional benefit: Zero external display library dependencies.
//
//   RAM: 1024-byte framebuffer. Flash: ~4 KB (including 5x7 font).
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

// Current operational state of Mochi
enum FaceState {
  FACE_BOOT,        // Booting up
  FACE_PORTAL,      // Configuration hotspot portal active
  FACE_IDLE,        // Awaiting user interaction / neutral
  FACE_LISTENING,   // Recording / listening to user speech
  FACE_THINKING,    // Gemini is generating response
  FACE_SPEAKING,    // Playing back spoken response
  FACE_WAITING,     // Waiting (quota limit / network retry)
  FACE_ERROR,

  // Kawaii Pet & Emotion States
  FACE_NEUTRAL,     // Content baseline
  FACE_HAPPY,       // Sweet smile + blush
  FACE_CUDDLE,      // Heart eyes + purring blush
  FACE_ANGRY,       // Furious / rapid tap reaction (X or jagged eyes)
  FACE_DIZZY,       // Shaken / spiral eyes
  FACE_SLEEPY,      // Flipped upside down or tired
  FACE_ECSTATIC     // Big sparkle eyes
};

// Safe to call even if OLED is disconnected — begin() returns false,
// and all drawing routines become safe no-ops without blocking execution.
bool faceBegin(int sda, int scl, uint8_t addr = 0x3C);
bool faceOk();

// Select SH1106 (1.3") vs SSD1306 (0.96") — mismatched panel displays garbage.
// Call before faceBegin(). Defaults to SH1106.
void faceSetPanel(bool sh1106);
bool faceIsSH1106();

void faceSetState(FaceState s);
FaceState faceGetState();

// ───────────────────── Screens ─────────────────────
enum FaceScreen {
  SCR_FACE,
  SCR_CLOCK,
  SCR_WEATHER,
  SCR_POMO,
  SCR_TIMER,
  SCR_STOPWATCH,
  SCR_COUNT
};

void       faceSetScreen(FaceScreen s);
FaceScreen faceScreen();
void       faceNextScreen();
void       faceRedraw();

// Squish physics: set pixel displacement from IMU tilt
void       faceSetSquish(int offX, int offY);


// ── Screen Data Providers ──
void faceClockData(int h24, int mi, int se, int day, int mon, int year,
                   int dow, bool rtcOk);
// Weather — provide condition as WMO code; face.cpp selects the bitmap
void faceWeatherData(bool valid, float tempC, int hum, int wmoCode, float windKmh);
void facePomoData(int secLeft, bool running, bool isBreak, int roundsDone);
void facePomoDataExt(int secLeft, int totalSec, bool running, int phase, int round, const char* presetLabel);
void faceStopwatchData(uint32_t elapsedMs, bool running);


// ───────────────────── Timer ─────────────────────
// Pomodoro is fixed at 25/5 min; the custom timer is adjustable —
// tapped on Touch-2 while on the timer screen to adjust minutes.
enum TimerMode {
  TM_IDLE,     // Timer unconfigured
  TM_SET,      // Setting duration — each tap adds +5 min
  TM_RUN,      // Countdown running
  TM_PAUSE,    // Paused with remaining time preserved
  TM_DONE      // Completed — beeped, display flashing
};

// secLeft: remaining seconds | totalSec: initial duration (for progress bar)
// setMin : displayed minutes while in TM_SET mode
// blink  : whether to render numerals during TM_DONE flash cycle
void faceTimerData(int secLeft, int totalSec, int setMin,
                   TimerMode mode, bool blink);

// ───────────────── Bottom Status Line ─────────────────
// ⚠️ Enum order MUST match the BN_MSG array in banglabmp.h
// (Both generated from the MSGS list in gen_bangla.py)
enum FaceMsg {
  MSG_NONE, MSG_BOLUN, MSG_SHUNCHHI, MSG_BHABCHHI, MSG_BOLCHHI,
  MSG_JUKTECHHI, MSG_KOTA, MSG_WIFI_NEI, MSG_KEY_NEI, MSG_SEC_POR,
  MSG_SOMOSSA, MSG_OPEKKHA
};

void faceSetMsg(FaceMsg m);
void faceSetWait(int seconds);        // Formats "After N seconds"

// ⭐ Speech Lip-Sync — level 0..255 representing audio amplitude.
// Updates only the mouth window, keeping CPU and I2C overhead minimal.
void faceMouth(uint8_t level);

// Microphone VU meter during recording (uses the same efficient window update)
void faceMicLevel(uint8_t level);

// Call from loop() — handles eye blinks, thinking animation dots, and timers
void faceTick();

// ── Test Hook Only ──
// Exposes the framebuffer buffer for desktop automated testing to verify
// mouth sizing and geometry without hardware. Excluded in firmware builds.
#ifdef FACE_TEST_HOOKS
const uint8_t *faceBuffer();          // 128*8 byte
#endif
