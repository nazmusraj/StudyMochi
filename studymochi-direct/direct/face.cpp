#include "face.h"
#include "rtcclock.h"   // banglaDigits()
#include "banglabmp.h" // barer nam ar chhoto shonkha — bitmap
#include <Wire.h>
#include <esp_random.h>


// ───────────────────────── ছবির বাফার ─────────────────────────
// SSD1306 ১২৮×৬৪ = ৮ পেজ × ১২৮ কলাম। এক বাইট = এক কলামের ৮ পিক্সেল।
#define W 128
#define H 64
#define PAGES (H / 8)

static uint8_t   fb[W * PAGES];
static bool      gOk   = false;
static uint8_t   gAddr = 0x3C;
static FaceState gState = FACE_BOOT;

// ⚠️ SSD1306 না SH1106 — এটাই সবচেয়ে বড় ফাঁদ।
// বাজারের অনেক "SSD1306" আসলে SH1106 (বিশেষত ১.৩ ইঞ্চি)। SH1106
// 0x21/0x22 (horizontal addressing) কমান্ড চেনে না, তাই পুরো ছবিটা
// একটামাত্র পেজে গিয়ে পড়ে আর বাকি পর্দায় আবর্জনা থেকে যায়।
//
// সমাধান: **page addressing** — এটা দুটো কন্ট্রোলারেই চলে।
// তফাত থাকে মোটে দুটো: কলামের অফসেট (SH1106-এর RAM ১৩২ চওড়া,
// দেখা যায় ২..১২৯) আর চার্জ পাম্পের কমান্ড।
static bool    gSH1106  = true;      // o কমান্ড দিয়ে বদলানো যায়
static uint8_t gColOff  = 2;

// ঠোঁটের জানালা — শুধু এটুকুই বারবার পাঠাই
#define MOUTH_X0 40
#define MOUTH_X1 88
#define MOUTH_P0 5            // y 40..55
#define MOUTH_P1 6

// লেভেল বারের জানালা (শোনার সময়)
#define BAR_X0 14
#define BAR_X1 114
#define BAR_P0 6
#define BAR_P1 6

// ───────────────────────── ৫×৭ ফন্ট ─────────────────────────
// ASCII 32..126, প্রতি অক্ষরে ৫ বাইট (এক বাইট = এক কলাম)
static const uint8_t FONT[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00,
  0x14,0x7F,0x14,0x7F,0x14, 0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62,
  0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00, 0x00,0x1C,0x22,0x41,0x00,
  0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
  0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00,
  0x20,0x10,0x08,0x04,0x02, 0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00,
  0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31, 0x18,0x14,0x12,0x7F,0x10,
  0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
  0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00,
  0x00,0x56,0x36,0x00,0x00, 0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14,
  0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06, 0x32,0x49,0x79,0x41,0x3E,
  0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
  0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01,
  0x3E,0x41,0x49,0x49,0x7A, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
  0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
  0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
  0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
  0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
  0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63,
  0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43, 0x00,0x7F,0x41,0x41,0x00,
  0x02,0x04,0x08,0x10,0x20, 0x00,0x41,0x41,0x7F,0x00, 0x04,0x02,0x01,0x02,0x04,
  0x40,0x40,0x40,0x40,0x40, 0x00,0x01,0x02,0x04,0x00, 0x20,0x54,0x54,0x54,0x78,
  0x7F,0x48,0x44,0x44,0x38, 0x38,0x44,0x44,0x44,0x20, 0x38,0x44,0x44,0x48,0x7F,
  0x38,0x54,0x54,0x54,0x18, 0x08,0x7E,0x09,0x01,0x02, 0x0C,0x52,0x52,0x52,0x3E,
  0x7F,0x08,0x04,0x04,0x78, 0x00,0x44,0x7D,0x40,0x00, 0x20,0x40,0x44,0x3D,0x00,
  0x7F,0x10,0x28,0x44,0x00, 0x00,0x41,0x7F,0x40,0x00, 0x7C,0x04,0x18,0x04,0x78,
  0x7C,0x08,0x04,0x04,0x78, 0x38,0x44,0x44,0x44,0x38, 0x7C,0x14,0x14,0x14,0x08,
  0x08,0x14,0x14,0x18,0x7C, 0x7C,0x08,0x04,0x04,0x08, 0x48,0x54,0x54,0x54,0x20,
  0x04,0x3F,0x44,0x40,0x20, 0x3C,0x40,0x40,0x20,0x1C, 0x1C,0x20,0x40,0x20,0x1C,
  0x3C,0x40,0x30,0x40,0x3C, 0x44,0x28,0x10,0x28,0x44, 0x0C,0x50,0x50,0x50,0x3C,
  0x44,0x64,0x54,0x4C,0x44, 0x00,0x08,0x36,0x41,0x00, 0x00,0x00,0x77,0x00,0x00,
  0x00,0x41,0x36,0x08,0x00, 0x02,0x01,0x02,0x04,0x02,
};

// ───────────────────── পর্দার তথ্য ─────────────────────
static FaceScreen gScreen = SCR_FACE;

static struct { int h24, mi, se, day, mon, year, dow; bool ok; } gClk =
  {0,0,0,1,1,2026,0,false};
static struct { bool valid; float t; int hum, code; float wind; } gWx =
  {false, 0, 0, -1, 0};
static struct { int secLeft; bool run, brk; int rounds; } gPomo = {25*60,false,false,0};
static struct { int secLeft, totalSec, setMin; TimerMode mode; bool blink; }
  gTmr = {0, 0, 0, TM_IDLE, true};

// ───────────────────────── I2C ─────────────────────────
static void cmd(uint8_t c) {
  Wire.beginTransmission(gAddr);
  Wire.write((uint8_t)0x00);
  Wire.write(c);
  Wire.endTransmission();
}

// একটা আয়তক্ষেত্র পাঠাই — এটাই আসল কৌশল, পুরো পর্দা নয়।
// প্রতি পেজে আলাদা করে page+column বসাই (0xB0 / 0x00 / 0x10) —
// এই তিনটে কমান্ড SSD1306 আর SH1106 **দুটোতেই** এক রকম কাজ করে।
static void pushWindow(uint8_t x0, uint8_t x1, uint8_t p0, uint8_t p1) {
  if (!gOk) return;
  if (x1 >= W) x1 = W - 1;
  if (p1 >= PAGES) p1 = PAGES - 1;

  for (uint8_t p = p0; p <= p1; p++) {
    uint8_t col = x0 + gColOff;
    Wire.beginTransmission(gAddr);
    Wire.write((uint8_t)0x00);
    Wire.write(0xB0 | p);                 // কোন পেজ
    Wire.write(0x00 | (col & 0x0F));      // কলামের নিচের চার বিট
    Wire.write(0x10 | (col >> 4));        // উপরের চার বিট
    Wire.endTransmission();

    const uint8_t *src = fb + p * W + x0;
    uint16_t n = x1 - x0 + 1;
    while (n) {
      uint16_t k = n > 16 ? 16 : n;                   // I2C বাফারে আঁটে
      Wire.beginTransmission(gAddr);
      Wire.write((uint8_t)0x40);
      Wire.write(src, k);
      Wire.endTransmission();
      src += k; n -= k;
    }
  }
}

static void pushAll() { pushWindow(0, W - 1, 0, PAGES - 1); }

// ───────────────────────── আঁকার সরঞ্জাম ─────────────────────────
static inline void px(int x, int y, bool on) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  uint8_t *b = &fb[(y >> 3) * W + x];
  uint8_t  m = 1 << (y & 7);
  if (on) *b |= m; else *b &= ~m;
}

static void clearRect(int x, int y, int w, int h) {
  for (int i = x; i < x + w; i++)
    for (int j = y; j < y + h; j++) px(i, j, false);
}

static void fillRect(int x, int y, int w, int h, bool on = true) {
  for (int i = x; i < x + w; i++)
    for (int j = y; j < y + h; j++) px(i, j, on);
}

static void fillCircle(int cx, int cy, int r, bool on = true) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) px(cx + x, cy + y, on);
}

// ভরা উপবৃত্ত — ঠোঁটের জন্য
static void fillEllipse(int cx, int cy, int rx, int ry, bool on = true) {
  if (rx < 1) rx = 1;
  if (ry < 1) ry = 1;
  for (int y = -ry; y <= ry; y++)
    for (int x = -rx; x <= rx; x++)
      if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
        px(cx + x, cy + y, on);
}

static void drawChar(int x, int y, char c) {
  if (c < 32 || c > 126) c = '?';
  const uint8_t *g = FONT + (c - 32) * 5;
  for (int i = 0; i < 5; i++) {
    uint8_t col = pgm_read_byte(g + i);
    for (int j = 0; j < 7; j++)
      if (col & (1 << j)) px(x + i, y + j, true);
  }
}

static void drawText(int x, int y, const char *s) {
  while (*s && x < W - 5) { drawChar(x, y, *s++); x += 6; }
}

static void drawTextCentered(int y, const char *s) {
  int n = strlen(s);
  int x = (W - n * 6) / 2;
  if (x < 0) x = 0;
  drawText(x, y, s);
}

// ───────────────── বিটম্যাপ (Adafruit-এর ধরন) ─────────────────
static void drawBmp(int x, int y, const uint8_t *bmp, int w, int h) {
  int bpr = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      uint8_t b = pgm_read_byte(bmp + j * bpr + (i >> 3));
      if (b & (0x80 >> (i & 7))) px(x + i, y + j, true);
    }
}

// বারের নাম — U8g2 নয়, আগেই বানানো ছবি (যুক্তাক্ষর ভাঙে না)
static void drawDayName(int y, int dow) {
  dow = (dow % 7 + 7) % 7;
  int w = BN_DAY_W[dow];
  const uint8_t *bmp = (const uint8_t *)pgm_read_ptr(&BN_DAY[dow]);
  drawBmp((W - w) / 2, y, bmp, w, BN_DAY_H);
}

// ছোট বাংলা সংখ্যা। ইনপুট সাধারণ ASCII ("07/09/2026") — ০-৯
// বিটম্যাপ থেকে আসে, বাকি চিহ্ন (: / %) ছোট রোমান ফন্ট থেকে।
static int bnNumWidth(const char *s) {
  int w = 0;
  for (; *s; s++) w += (*s >= '0' && *s <= '9') ? BN_NUM_W[*s - '0'] + 2 : 6;
  return w > 0 ? w - 1 : 0;
}

static void bnNum(int x, int y, const char *s) {          // y = উপরের কিনারা
  for (; *s; s++) {
    if (*s >= '0' && *s <= '9') {
      int d = *s - '0';
      drawBmp(x, y, (const uint8_t *)pgm_read_ptr(&BN_NUM[d]), BN_NUM_W[d], BN_NUM_H);
      x += BN_NUM_W[d] + 2;
    } else {
      drawChar(x, y + 3, *s);
      x += 6;
    }
  }
}

static void bnNumCentered(int y, const char *s) {
  int x = (W - bnNumWidth(s)) / 2;
  if (x < 0) x = 0;
  bnNum(x, y, s);
}

// বড় বাংলা সংখ্যা — ঘড়ির মূল সময় আর পমোডোরোর কাউন্টডাউন
static int bnBigWidth(const char *s) {
  int w = 0;
  for (; *s; s++) {
    if (*s >= '0' && *s <= '9') w += BN_BIG_W[*s - '0'] + 2;
    else                        w += BN_BCOLON_W + 4;
  }
  return w > 0 ? w - 2 : 0;
}

static void bnBig(int x, int y, const char *s) {
  for (; *s; s++) {
    if (*s >= '0' && *s <= '9') {
      int d = *s - '0';
      drawBmp(x, y, (const uint8_t *)pgm_read_ptr(&BN_BIG[d]), BN_BIG_W[d], BN_BIG_H);
      x += BN_BIG_W[d] + 2;
    } else {
      drawBmp(x + 2, y, BN_BCOLON, BN_BCOLON_W, BN_BIG_H);
      x += BN_BCOLON_W + 4;
    }
  }
}

static void bnBigCentered(int y, const char *s) {
  int x = (W - bnBigWidth(s)) / 2;
  if (x < 0) x = 0;
  bnBig(x, y, s);
}

// আবহাওয়ার কথাটা — WMO কোড থেকে কোন ছবি
static int wxLabel(int code) {
  switch (code) {
    case 0:  return 0;   case 1:  return 1;   case 2:  return 2;
    case 3:  return 3;
    case 45: case 48: return 4;
    case 51: case 53: case 55: return 5;
    case 56: case 57: return 6;
    case 61: return 7;   case 63: return 8;   case 65: return 9;
    case 66: case 67: return 10;
    case 71: case 73: case 75: case 77: return 11;
    case 80: return 12;  case 81: return 13;  case 82: return 14;
    case 85: case 86: return 15;
    case 95: return 16;
    case 96: case 99: return 17;
    default: return 18;
  }
}

static void drawWxLabel(int y, int code) {
  int i = wxLabel(code);
  int w = BN_WX_W[i];
  drawBmp((W - w) / 2, y, (const uint8_t *)pgm_read_ptr(&BN_WX[i]), w, BN_WX_H);
}

// ════════════════════════════════════════════════════════════════
//   মোচির মুখ — kawaii ভঙ্গি
//
//   আপনার পাঠানো ইরেজারগুলোর ছবি ধরে ছয়টা ভঙ্গি বানানো, আর
//   প্রতিটাকে মোচির এক-একটা অবস্থার সাথে জুড়ে দেওয়া হয়েছে।
//   তাই পর্দা দেখেই বোঝা যায় সে কী করছে — কিছু পড়তে হয় না:
//
//     ফাঁকা আছে   ⟶  বড় গোল চোখ + মিষ্টি হাসি + গালে লালচে ছোপ
//     শুনছে       ⟶  তারা-চোখ (মন দিয়ে শুনছে), ছোট হাঁ
//     ভাবছে       ⟶  −_−  সরু চোখ, নিচে তিনটে বিন্দু ঘোরে
//     বলছে        ⟶  ^ω^  খুশি চোখ, ঠোঁট কথার সাথে নড়ে
//     অপেক্ষা      ⟶  T_T  কাঁদছে, চোখের নিচে জলের ফোঁটা
//     সমস্যা       ⟶  >_<  চোখ কুঁচকে
// ════════════════════════════════════════════════════════════════
#define EYE_L 44
#define EYE_R 84
#define EYE_Y 26
#define MOUTH_CX 64
#define MOUTH_CY 47

// ── পর্দার জায়গা ভাগ করা (১২৮×৬৪) ──
//   পেজ ০-১  (y  0..15) : নিচের-লাইনের লেখা — উপরে বসে
//   পেজ ২-৪  (y 16..39) : চোখ, গাল, জলের ফোঁটা
//   পেজ ৫-৬  (y 40..55) : ঠোঁট
//   পেজ ৭    (y 56..63) : ফাঁকা
// আগে লেখা আর হাসি একই জায়গায় পড়ত, একটা আরেকটাকে মুছে দিত।
#define TXTWIN_P0 0
#define TXTWIN_P1 1
#define EYEWIN_X0 20
#define EYEWIN_X1 108
#define EYEWIN_P0 2
#define EYEWIN_P1 4

static bool     gBlink = false;
static uint32_t gBlinkAt = 0;
static bool     gWink = false;          // এক চোখ বন্ধ — মাঝে মাঝে
static uint32_t gWinkAt = 0;
static uint8_t  gMouth = 0;
static uint8_t  gSpin  = 0;
static FaceMsg  gMsg   = MSG_NONE;
static int      gWaitSec = 0;

enum EyeStyle { EYE_ROUND, EYE_SPARKLE, EYE_HAPPY, EYE_FLAT, EYE_CRY, EYE_SQUINT };

static EyeStyle eyeFor(FaceState s) {
  switch (s) {
    case FACE_LISTENING: return EYE_SPARKLE;
    case FACE_THINKING:  return EYE_FLAT;
    case FACE_SPEAKING:  return EYE_HAPPY;
    case FACE_WAITING:   return EYE_CRY;
    case FACE_ERROR:     return EYE_SQUINT;
    default:             return EYE_ROUND;
  }
}

// ── এক লাইন (তির্যক টানের জন্য) ──
static void line(int x0, int y0, int x1, int y1) {
  int dx = x1 - x0, dy = y1 - y0;
  int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
        ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
  if (n == 0) { px(x0, y0, true); return; }
  for (int i = 0; i <= n; i++) {
    px(x0 + dx * i / n, y0 + dy * i / n, true);
    px(x0 + dx * i / n, y0 + dy * i / n + 1, true);   // ২ পিক্সেল মোটা
  }
}

// ── একটা চোখ ──
static void drawEye(int cx, int cy, EyeStyle st, bool closed) {
  if (closed) {                                  // পলক — নিচু বাঁক
    for (int dx = -6; dx <= 6; dx++) {
      int y = cy + 1 - (36 - dx * dx) / 30;
      px(cx + dx, y, true); px(cx + dx, y + 1, true);
    }
    return;
  }
  switch (st) {
    case EYE_ROUND:
      fillCircle(cx, cy, 7);
      fillCircle(cx + 3, cy - 3, 2, false);       // ঝলক
      fillCircle(cx - 2, cy + 3, 1, false);       // ছোট ঝলক
      break;

    case EYE_SPARKLE:
      fillCircle(cx, cy, 7);
      // চার-কোণা তারা — মন দিয়ে শোনার চোখ
      for (int d = -3; d <= 3; d++) {
        int t = 3 - (d < 0 ? -d : d);
        for (int k = -t; k <= t; k++) px(cx + d + 1, cy - 2 + k, false);
      }
      fillCircle(cx - 3, cy + 3, 1, false);
      break;

    case EYE_HAPPY:                               // ^ — খুশিতে বোজা
      for (int dx = -7; dx <= 7; dx++) {
        int y = cy + 2 - (7 - (dx < 0 ? -dx : dx));
        px(cx + dx, y, true); px(cx + dx, y + 1, true);
      }
      break;

    case EYE_FLAT:                                // − — ভাবছে
      fillRect(cx - 6, cy - 1, 13, 2);
      break;

    case EYE_CRY:                                 // T — কাঁদছে
      fillRect(cx - 6, cy - 6, 13, 2);
      fillRect(cx - 1, cy - 6, 3, 12);
      fillCircle(cx + 6, cy + 9, 2);              // জলের ফোঁটা
      px(cx + 6, cy + 6, true);
      break;

    case EYE_SQUINT:                              // >  <
      if (cx == EYE_L) { line(cx - 5, cy - 5, cx + 4, cy); line(cx - 5, cy + 5, cx + 4, cy); }
      else             { line(cx + 5, cy - 5, cx - 4, cy); line(cx + 5, cy + 5, cx - 4, cy); }
      break;
  }
}

// ── গালে লালচে ছোপ — তিনটে ছোট তির্যক টান ──
static void drawBlush(int cx, int cy) {
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < 5; k++) {
      px(cx + i * 5 + k,     cy + 4 - k, true);
      px(cx + i * 5 + k + 1, cy + 4 - k, true);   // ২ পিক্সেল মোটা
    }
}

static bool blushFor(FaceState s) {
  return s == FACE_IDLE || s == FACE_LISTENING || s == FACE_SPEAKING;
}

static void drawEyes() {
  clearRect(EYEWIN_X0, EYEWIN_P0 * 8, EYEWIN_X1 - EYEWIN_X0 + 1,
            (EYEWIN_P1 - EYEWIN_P0 + 1) * 8);
  EyeStyle st = eyeFor(gState);
  drawEye(EYE_L, EYE_Y, st, gBlink || gWink);
  drawEye(EYE_R, EYE_Y, st, gBlink);              // wink হলে ডান চোখ খোলা
  if (blushFor(gState)) {
    drawBlush(EYE_L - 22, EYE_Y + 8);
    drawBlush(EYE_R + 11, EYE_Y + 8);
  }
}

// ── ঠোঁট ──
// ধাপ ০ = বন্ধ (অবস্থা অনুযায়ী আকার), ১..৫ = যত জোরে কথা তত বড় হাঁ
static void drawMouth(uint8_t step) {
  clearRect(MOUTH_X0, MOUTH_P0 * 8, MOUTH_X1 - MOUTH_X0 + 1, 16);

  if (step > 0) {                                 // কথা বলছে — হাঁ
    int ry = 1 + step, rx = 10 + step / 2;
    fillEllipse(MOUTH_CX, MOUTH_CY, rx, ry);
    if (step >= 3) fillEllipse(MOUTH_CX, MOUTH_CY + 1, rx - 3, ry - 2, false);
    return;
  }

  switch (gState) {
    case FACE_SPEAKING:                           // ω — দুটো ছোট বাঁক
      for (int s2 = 0; s2 < 2; s2++)
        for (int dx = -4; dx <= 4; dx++) {
          int y = MOUTH_CY - (16 - dx * dx) / 8;
          int x = MOUTH_CX - 5 + s2 * 10 + dx;
          px(x, y, true); px(x, y + 1, true);
        }
      break;

    case FACE_LISTENING:                          // ছোট গোল হাঁ
      fillCircle(MOUTH_CX, MOUTH_CY, 4);
      fillCircle(MOUTH_CX, MOUTH_CY, 2, false);
      break;

    case FACE_THINKING:                           // ছোট চ্যাপ্টা মুখ
      fillRect(MOUTH_CX - 5, MOUTH_CY - 2, 11, 5);
      break;

    case FACE_WAITING:                            // উল্টো বাঁক — মন খারাপ
      for (int dx = -7; dx <= 7; dx++) {
        int y = MOUTH_CY + (49 - dx * dx) / 18;
        px(MOUTH_CX + dx, y, true); px(MOUTH_CX + dx, y + 1, true);
      }
      break;

    case FACE_ERROR:
      fillRect(MOUTH_CX - 6, MOUTH_CY - 3, 13, 7);
      fillRect(MOUTH_CX - 4, MOUTH_CY - 1, 9, 3, false);
      break;

    default:                                      // মিষ্টি হাসি
      for (int dx = -11; dx <= 11; dx++) {
        int y = MOUTH_CY + 4 - (121 - dx * dx) / 22;
        px(MOUTH_CX + dx, y, true); px(MOUTH_CX + dx, y + 1, true);
      }
      // দু'পাশে ছোট টোল — এতেই হাসিটা মিষ্টি লাগে
      px(MOUTH_CX - 13, MOUTH_CY - 2, true); px(MOUTH_CX - 13, MOUTH_CY - 1, true);
      px(MOUTH_CX + 13, MOUTH_CY - 2, true); px(MOUTH_CX + 13, MOUTH_CY - 1, true);
      break;
  }
}

// ── অবস্থার লেখা (বাংলা বিটম্যাপ) — পর্দার উপরে ──
static void drawBottomText() {
  clearRect(0, 0, W, 16);
  if (gMsg == MSG_NONE) return;

  if (gMsg == MSG_SEC_POR) {                      // "৩০০ সেকেন্ড পর"
    char n[8];
    snprintf(n, sizeof(n), "%d", gWaitSec);
    int nw = bnNumWidth(n), lw = BN_MSG_W[MSG_SEC_POR];
    int x = (W - (nw + 4 + lw)) / 2; if (x < 0) x = 0;
    bnNum(x, 2, n);
    drawBmp(x + nw + 4, 0, (const uint8_t *)pgm_read_ptr(&BN_MSG[MSG_SEC_POR]),
            lw, BN_MSG_H);
    return;
  }
  int w = BN_MSG_W[gMsg];
  drawBmp((W - w) / 2, 0, (const uint8_t *)pgm_read_ptr(&BN_MSG[gMsg]), w, BN_MSG_H);
}

// ───────────────────────── পর্দাগুলো ─────────────────────────
// ঘণ্টা দেখে দিনের ভাগ বাছি। ইংরেজি AM/PM-এর বদলে বাংলা শব্দ —
// বাংলা ঘড়িতে এটাই স্বাভাবিক, আর ১২-ঘণ্টার সময়টা কোন বেলার
// তা-ও পরিষ্কার হয়।
static uint8_t partIdx(int h24) {
  if (h24 < 4)  return 0;      // রাত
  if (h24 < 6)  return 1;      // ভোর
  if (h24 < 12) return 2;      // সকাল
  if (h24 < 15) return 3;      // দুপুর
  if (h24 < 18) return 4;      // বিকাল
  if (h24 < 20) return 5;      // সন্ধ্যা
  return 0;                    // রাত
}
static const uint8_t *partBmp(int h24) {
  return (const uint8_t *)pgm_read_ptr(&BN_PART[partIdx(h24)]);
}
static uint8_t partW(int h24) { return BN_PART_W[partIdx(h24)]; }

static void drawClockScreen() {
  if (!gClk.ok) {
    drawBmp((W - BN_GHORI_NEI_W) / 2, 20, BN_GHORI_NEI, BN_GHORI_NEI_W, BN_GHORI_NEI_H);
    drawTextCentered(44, "DS3231: SDA21 SCL22");
    return;
  }

  // ── তারিখ: ছোট সংখ্যায়, একদম উপরে ──
  char d[24];
  snprintf(d, sizeof(d), "%02d/%02d/%04d", gClk.day, gClk.mon, gClk.year);
  bnNumCentered(0, d);

  // ── সময়: বড় করে, মাঝখানে। এটাই পর্দার মূল জিনিস ──
  int h12 = gClk.h24 % 12; if (h12 == 0) h12 = 12;
  char t[12], sec[6];
  snprintf(t,   sizeof(t),   "%02d:%02d", h12, gClk.mi);
  snprintf(sec, sizeof(sec), "%02d", gClk.se);

  int tw = bnBigWidth(t), sw = bnNumWidth(sec);
  int tx = (W - (tw + 5 + sw)) / 2; if (tx < 0) tx = 0;
  bnBig(tx, 15, t);                            // ঘণ্টা:মিনিট
  bnNum(tx + tw + 5, 15, sec);                 // সেকেন্ড — ছোট, উপরে
  // ── দিনের ভাগ: "সকাল / দুপুর / বিকাল..." — AM/PM নয় ──
  drawBmp(tx + tw + 5, 28, partBmp(gClk.h24), partW(gClk.h24), BN_PART_H);

  // ── বার: বিটম্যাপ, নিচে ──
  drawDayName(H - BN_DAY_H + 1, gClk.dow);
}

static void drawWeatherScreen() {
  if (!gWx.valid) {
    drawBmp((W - BN_WX_ANCHHI_W) / 2, 20, BN_WX_ANCHHI, BN_WX_ANCHHI_W, BN_WX_ANCHHI_H);
    drawBmp((W - BN_MSG_W[MSG_WIFI_NEI]) / 2, 42,
            (const uint8_t *)pgm_read_ptr(&BN_MSG[MSG_WIFI_NEI]),
            BN_MSG_W[MSG_WIFI_NEI], BN_MSG_H);
    return;
  }
  // তাপমাত্রা — বড় সংখ্যা, পাশে ডিগ্রির চিহ্ন
  char t[12];
  snprintf(t, sizeof(t), "%d", (int)(gWx.t + 0.5f));
  int tw = bnBigWidth(t);
  int tx = (W - (tw + 10)) / 2; if (tx < 0) tx = 0;
  bnBig(tx, 2, t);
  fillCircle(tx + tw + 5, 8, 3);               // °
  fillCircle(tx + tw + 5, 8, 1, false);
  drawText(tx + tw + 10, 12, "C");

  drawWxLabel(26, gWx.code);                   // কথাটা — বিটম্যাপ

  // আর্দ্রতা — ছোট করে নিচে
  char hu[12];
  snprintf(hu, sizeof(hu), "%d", gWx.hum);
  int hw = BN_ARDROTA_W + 4 + bnNumWidth(hu) + 7;
  int hx = (W - hw) / 2; if (hx < 0) hx = 0;
  drawBmp(hx, 47, BN_ARDROTA, BN_ARDROTA_W, BN_ARDROTA_H);
  bnNum(hx + BN_ARDROTA_W + 4, 50, hu);
  drawText(hx + BN_ARDROTA_W + 6 + bnNumWidth(hu), 53, "%");
}

static void drawPomoScreen() {
  // ⚠️ এখানে শুধু সময়টাই লেখা — "পড়ার সময়" লেখা বাদ, কারণ
  //    যুক্তাক্ষর ভাঙার ঝুঁকি নেওয়ার দরকার নেই। বিরতির সময় শুধু
  //    "বিরতি" শব্দটা ওপরে বসে, তাতেই বোঝা যায় কোন পর্যায়।
  if (gPomo.brk)
    drawBmp((W - BN_BIROTI_W) / 2, 0, BN_BIROTI, BN_BIROTI_W, BN_BIROTI_H);

  // ⚠️ উচ্চতার হিসাব — ৬৪ পিক্সেলে সব আঁটতে হবে, নইলে নিচের
  //    লেখার "ু"-কার কেটে যায় (আগে ঠিক সেটাই হচ্ছিল):
  //      y  0..13  "বিরতি" (শুধু বিরতির সময়)
  //      y 16..29  সময় — বড় সংখ্যা
  //      y 34..39  অগ্রগতির বার
  //      y 46..59  "চলছে" / "ছুঁয়ে ধরুন"
  char t[12];
  int pm = gPomo.secLeft / 60, ps = gPomo.secLeft % 60;
  if (pm > 99) pm = 99;
  if (pm < 0) { pm = 0; ps = 0; }
  snprintf(t, sizeof(t), "%02d:%02d", pm, ps);
  bnBigCentered(13, t);

  // অগ্রগতির বার
  int total = gPomo.brk ? 5 * 60 : 25 * 60;
  int w = (W - 24) * (total - gPomo.secLeft) / (total ? total : 1);
  if (w < 0) w = 0;
  if (w > W - 24) w = W - 24;
  fillRect(12, 37, W - 24, 1);
  fillRect(12, 34, w, 6);

  if (gPomo.run)
    drawBmp((W - BN_CHOLCHHE_W) / 2, 45, BN_CHOLCHHE, BN_CHOLCHHE_W, BN_CHOLCHHE_H);
  else
    drawBmp((W - BN_CHEPE_W) / 2, 45, BN_CHEPE, BN_CHEPE_W, BN_CHEPE_H);
}

// ───────────────────── টাইমারের পর্দা ─────────────────────
// পমোডোরোর মতোই সাজ, তাই চেনা লাগবে। তফাত শুধু উপরের শব্দটা
// ("টাইমার") আর নিচের লাইনটা — কোন ভঙ্গিতে আছি সেটা ওখানেই বলা।
//
// ⚠️ উচ্চতার হিসাব (৬৪ পিক্সেলে সব আঁটতে হবে):
//      y  2..14  "টাইমার"
//      y 18..31  সময় — বড় সংখ্যা
//      y 34..39  অগ্রগতির বার
//      y 46..59  নিচের লাইন
//    মাঝের ফাঁকা সারিগুলো ইচ্ছে করেই রাখা — "টাইমার"-এর নিচে
//    আগে মাত্র এক সারি ফাঁকা ছিল, দেখতে গায়ে-গায়ে লাগছিল।
static void drawTimerScreen() {
  drawBmp((W - BN_TIMER_W) / 2, 0, BN_TIMER, BN_TIMER_W, BN_TIMER_H);

  // ── মাঝের বড় সংখ্যা ──
  // বসানোর ভঙ্গিতে "১৫ মিনিট", বাকি সব সময় "MM:SS"
  if (gTmr.mode == TM_SET) {
    char m[8];
    snprintf(m, sizeof(m), "%d", gTmr.setMin);
    int mw = bnBigWidth(m);
    int mx = (W - (mw + 4 + BN_MINIT_W)) / 2; if (mx < 0) mx = 0;
    bnBig(mx, 15, m);
    drawBmp(mx + mw + 4, 18, BN_MINIT, BN_MINIT_W, BN_MINIT_H);
  } else if (gTmr.mode != TM_DONE || gTmr.blink) {
    // TM_DONE-এ blink false হলে সংখ্যাটা এই ডাকে আঁকি না — তাতেই
    // পর্দা জ্বলে-নেভে। আলাদা কোনো টাইমার লাগে না।
    char t[12];
    int mm = gTmr.secLeft / 60, ss = gTmr.secLeft % 60;
    if (mm > 99) mm = 99;
    if (mm < 0) { mm = 0; ss = 0; }
    snprintf(t, sizeof(t), "%02d:%02d", mm, ss);
    bnBigCentered(15, t);
  }

  // ── অগ্রগতির বার — কতটা পেরিয়েছে ──
  if (gTmr.mode == TM_RUN || gTmr.mode == TM_PAUSE) {
    int tot = gTmr.totalSec > 0 ? gTmr.totalSec : 1;
    int gone = tot - gTmr.secLeft;
    if (gone < 0) gone = 0;
    int w = (W - 24) * gone / tot;
    if (w > W - 24) w = W - 24;
    fillRect(12, 37, W - 24, 1);
    fillRect(12, 34, w, 6);
  }

  // ── নিচের লাইন ──
  switch (gTmr.mode) {
    case TM_SET:
      drawBmp((W - BN_BOSAN_W) / 2, 45, BN_BOSAN, BN_BOSAN_W, BN_BOSAN_H);
      break;
    case TM_RUN:
      drawBmp((W - BN_CHOLCHHE_W) / 2, 45, BN_CHOLCHHE, BN_CHOLCHHE_W,
              BN_CHOLCHHE_H);
      break;
    case TM_PAUSE:
      drawBmp((W - BN_THAMANO_W) / 2, 45, BN_THAMANO, BN_THAMANO_W,
              BN_THAMANO_H);
      break;
    case TM_DONE:
      if (gTmr.blink)
        drawBmp((W - BN_SHESH_W) / 2, 45, BN_SHESH, BN_SHESH_W, BN_SHESH_H);
      break;
    default:                                    // TM_IDLE
      drawBmp((W - BN_CHEPE_W) / 2, 45, BN_CHEPE, BN_CHEPE_W, BN_CHEPE_H);
      break;
  }
}

// ⭐ মুখটা এখন পর্দায় আছে কি?
// এই একটা প্রশ্নের উত্তরেই সব ঠিক হয়। আগে faceTick() পর্দার কথা
// না ভেবেই চোখের পলক আর ভাবনার বিন্দু এঁকে পাঠিয়ে দিত — তাই
// পমোডোরো বা ঘড়ি চলার মাঝখানে হঠাৎ মুখ এসে পড়ত।
static bool showingFace() {
  if (gState == FACE_LISTENING || gState == FACE_THINKING ||
      gState == FACE_SPEAKING) return true;          // কাজে আছে — মুখ
  if (gState == FACE_IDLE)      return gScreen == SCR_FACE;
  return gState == FACE_WAITING;                     // অপেক্ষার পর্দাতেও মুখ
}

// ───────────────────────── পুরো পর্দা ─────────────────────────
static void redrawAll() {
  memset(fb, 0, sizeof(fb));

  if (!showingFace() && gState == FACE_IDLE) {
    if (gScreen == SCR_CLOCK)        drawClockScreen();
    else if (gScreen == SCR_WEATHER) drawWeatherScreen();
    else if (gScreen == SCR_TIMER)   drawTimerScreen();
    else                             drawPomoScreen();
    pushAll();
    return;
  }

  switch (gState) {
    case FACE_BOOT:
      drawBmp((W - BN_TITLE_W) / 2, 16, BN_TITLE, BN_TITLE_W, BN_TITLE_H);
      drawBmp((W - BN_CHALU_W) / 2, 40, BN_CHALU, BN_CHALU_W, BN_CHALU_H);
      pushAll();
      return;

    case FACE_PORTAL:
      drawBmp((W - BN_SETUP_W) / 2, 0, BN_SETUP, BN_SETUP_W, BN_SETUP_H);
      drawBmp((W - BN_PHONE_W) / 2, 22, BN_PHONE, BN_PHONE_W, BN_PHONE_H);
      // হটস্পটের নাম-পাসওয়ার্ড রোমানেই, কারণ ফোনে ঠিক এভাবেই দেখাবে
      drawTextCentered(44, "StudyMochi-Direct");
      drawTextCentered(54, "mochi1234");
      pushAll();
      return;


    default:                              // IDLE / LISTENING / THINKING / SPEAKING
      drawEyes();
      drawMouth(0);
      break;
  }
  drawBottomText();
  pushAll();
}

// ───────────────────────── বাইরের জন্য ─────────────────────────
bool faceBegin(int sda, int scl, uint8_t addr) {
  gAddr = addr;
  Wire.begin(sda, scl);
  Wire.setClock(400000);

  Wire.beginTransmission(gAddr);
  if (Wire.endTransmission() != 0) {
    // ⚠️ ঠিকানায় সাড়া নেই। বেশিরভাগ SSD1306 মডিউল 0x3C, কিছু 0x3D।
    //    তাই চুপ করে হাল না ছেড়ে পুরো বাসটা একবার খুঁজে দেখি —
    //    কী পাওয়া গেল সেটা Serial-এ বলে দিই, তাহলে আর আন্দাজ
    //    করতে হবে না।
    Serial.printf("[oled] 0x%02X-e sara nei. I2C bus khunjchi...\n", gAddr);
    uint8_t found = 0;
    for (uint8_t a = 1; a < 127; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) {
        Serial.printf("[oled]   0x%02X-e kichu ekta achhe\n", a);
        found = a;
      }
    }
    if (!found) {
      Serial.println("[oled] bus-e kichui nei — tar dekhun:");
      Serial.println("       VCC->3V3, GND->GND, SDA->GPIO21, SCL->GPIO22");
      gOk = false;
      return false;
    }
    // যা পেলাম সেটাই ব্যবহার করি — ঠিকানা নিয়ে আর ভাবতে হবে না
    Serial.printf("[oled] 0x%02X diye cheshta korchi\n", found);
    gAddr = found;
  }
  gOk = true;

  static const uint8_t init[] = {
    0xAE,             // ঘুমাও
    0xD5, 0x80,       // ক্লক
    0xA8, 0x3F,       // ৬৪ সারি
    0xD3, 0x00,       // অফসেট নেই
    0x40,             // শুরুর লাইন ০
    // চার্জ পাম্প — দুই কন্ট্রোলারের দুই কমান্ড, দুটোই পাঠাই।
    // যারটা নিজের নয় সেটা সে চুপচাপ ফেলে দেয়।
    0x8D, 0x14,       // SSD1306
    0xAD, 0x8B,       // SH1106
    0x20, 0x02,       // ⭐ page addressing — দুটোতেই নিরাপদ
    0xA1, 0xC8,       // ঠিকমুখো
    0xDA, 0x12,
    0x81, 0xCF,       // উজ্জ্বলতা
    0xD9, 0xF1,
    0xDB, 0x40,
    0xA4,             // RAM থেকেই দেখাও
    0xA6,             // স্বাভাবিক (উল্টো নয়)
    0xAF,             // জাগো
  };
  for (size_t i = 0; i < sizeof(init); i++) cmd(init[i]);
  Serial.printf("[oled] panel: %s (col offset %u)\n",
                gSH1106 ? "SH1106" : "SSD1306", gColOff);

  memset(fb, 0, sizeof(fb));
  pushAll();
  gState = FACE_BOOT;
  redrawAll();
  return true;
}

bool faceOk() { return gOk; }

// SH1106 আর SSD1306-এর মধ্যে বদল। ছবি ২ পিক্সেল সরে গেলে বা
// পর্দায় আবর্জনা থাকলে এটাই উল্টে দেখুন।
void faceSetPanel(bool sh1106) {
  gSH1106 = sh1106;
  gColOff = sh1106 ? 2 : 0;
  if (gOk) redrawAll();
}

bool faceIsSH1106() { return gSH1106; }

void faceSetState(FaceState s) {
  if (!gOk || s == gState) return;
  gState = s;
  gMouth = 0;
  gSpin = 0;
  redrawAll();
}

FaceState faceGetState() { return gState; }

void faceSetMsg(FaceMsg m) {
  if (m < 0 || m >= BN_MSG_N) m = MSG_NONE;
  if (m == gMsg) return;
  gMsg = m;
  if (!gOk || !showingFace()) return;   // অন্য পর্দার নিচের লাইন মুছব না
  drawBottomText();
  pushWindow(0, W - 1, TXTWIN_P0, TXTWIN_P1);
}

void faceSetWait(int seconds) {
  gWaitSec = seconds;
  gMsg = MSG_SEC_POR;
  if (!gOk || !showingFace()) return;
  drawBottomText();
  pushWindow(0, W - 1, TXTWIN_P0, TXTWIN_P1);
}

// ⭐ কথার সাথে ঠোঁট। level ০..২৫৫ → ধাপ ০..৫
void faceMouth(uint8_t level) {
  if (!gOk || !showingFace()) return;
  uint8_t step = level / 43;               // ২৫৫/৪৩ ≈ ৫
  if (step > 5) step = 5;
  if (step == gMouth) return;              // বদলায়নি — I2C বাঁচাই
  gMouth = step;
  drawMouth(step);
  pushWindow(MOUTH_X0, MOUTH_X1, MOUTH_P0, MOUTH_P1);   // ~৮২ বাইট
}

// শোনার সময় মাইকের লেভেল — নিচে একটা বার
void faceMicLevel(uint8_t level) {
  if (!gOk || !showingFace()) return;
  static uint8_t last = 255;
  uint8_t w = (uint8_t)((uint16_t)level * (BAR_X1 - BAR_X0) / 255);
  if (w == last) return;
  last = w;
  clearRect(BAR_X0, BAR_P0 * 8, BAR_X1 - BAR_X0 + 1, 8);
  fillRect(BAR_X0, BAR_P0 * 8 + 2, w, 4);
  pushWindow(BAR_X0, BAR_X1, BAR_P0, BAR_P1);
}

void faceSetScreen(FaceScreen s) {
  if (s < 0 || s >= SCR_COUNT) s = SCR_FACE;
  if (s == gScreen) return;
  gScreen = s;
  if (gOk) redrawAll();
}

FaceScreen faceScreen() { return gScreen; }

void faceNextScreen() {
  faceSetScreen((FaceScreen)((gScreen + 1) % SCR_COUNT));
}

void faceRedraw() { if (gOk) redrawAll(); }

void faceClockData(int h24, int mi, int se, int day, int mon, int year,
                   int dow, bool rtcOk) {
  gClk.h24 = h24; gClk.mi = mi; gClk.se = se;
  gClk.day = day; gClk.mon = mon; gClk.year = year;
  gClk.dow = dow; gClk.ok = rtcOk;
}

void faceWeatherData(bool valid, float tempC, int hum, int wmoCode, float windKmh) {
  gWx.valid = valid; gWx.t = tempC; gWx.hum = hum;
  gWx.code = wmoCode; gWx.wind = windKmh;
}

void faceTimerData(int secLeft, int totalSec, int setMin,
                   TimerMode mode, bool blink) {
  gTmr.secLeft  = secLeft;
  gTmr.totalSec = totalSec;
  gTmr.setMin   = setMin;
  gTmr.mode     = mode;
  gTmr.blink    = blink;
}

void facePomoData(int secLeft, bool running, bool isBreak, int roundsDone) {
  gPomo.secLeft = secLeft; gPomo.run = running;
  gPomo.brk = isBreak; gPomo.rounds = roundsDone;
}

void faceTick() {
  if (!gOk) return;
  if (!showingFace()) return;         // ⚠️ ঘড়ি/পমোডোরোর ওপর মুখ আঁকব না
  uint32_t now = millis();

  // ── চোখের পলক ──
  if (gState == FACE_IDLE || gState == FACE_SPEAKING ||
      gState == FACE_LISTENING) {
    if (!gBlink && now > gBlinkAt) {
      gBlink = true;  gBlinkAt = now + 120;
      drawEyes(); pushWindow(EYEWIN_X0, EYEWIN_X1, EYEWIN_P0, EYEWIN_P1);
    } else if (gBlink && now > gBlinkAt) {
      gBlink = false; gBlinkAt = now + 2200 + (esp_random() % 2500);
      drawEyes(); pushWindow(EYEWIN_X0, EYEWIN_X1, EYEWIN_P0, EYEWIN_P1);
    }
  }

  // ── মাঝে মাঝে এক চোখ টিপে দেয় — শুধু ফাঁকা থাকলে ──
  if (gState == FACE_IDLE && !gBlink) {
    if (!gWink && now > gWinkAt) {
      gWink = true;  gWinkAt = now + 260;
      drawEyes(); pushWindow(EYEWIN_X0, EYEWIN_X1, EYEWIN_P0, EYEWIN_P1);
    } else if (gWink && now > gWinkAt) {
      gWink = false; gWinkAt = now + 7000 + (esp_random() % 9000);
      drawEyes(); pushWindow(EYEWIN_X0, EYEWIN_X1, EYEWIN_P0, EYEWIN_P1);
    }
  } else if (gWink) {
    gWink = false;
  }

  // ── ভাবছে: তিনটে বিন্দু ঘোরে ──
  if (gState == FACE_THINKING) {
    static uint32_t next = 0;
    if (now > next) {
      next = now + 220;
      gSpin = (gSpin + 1) % 4;
      clearRect(MOUTH_X0, MOUTH_P0 * 8, MOUTH_X1 - MOUTH_X0 + 1, 16);
      for (int i = 0; i < 3; i++)
        fillCircle(MOUTH_CX - 14 + i * 14, MOUTH_CY, i == gSpin ? 4 : 2);
      pushWindow(MOUTH_X0, MOUTH_X1, MOUTH_P0, MOUTH_P1);
    }
  }
}

#ifdef FACE_TEST_HOOKS
const uint8_t *faceBuffer() { return fb; }
#endif
