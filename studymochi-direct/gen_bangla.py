#!/usr/bin/env python3
"""
Generates banglabmp.h — all Bengali text for StudyMochi pre-rendered as bitmaps.

Why this script: U8g2's unifont cannot properly render Bengali conjuncts/ligatures
("Study time", "Thursday", "Humidity" — all break). Therefore, texts are
pre-rendered using Noto Sans Bengali and stored as PROGMEM bitmaps.

If new text is needed, add to the lists below and run again:
    python3 gen_bangla.py > /dev/null
"""
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/noto/NotoSansBengali-Bold.ttf"
OUT = "/home/claude/direct/banglabmp.h"

# ── Day names (14px) ──
DAYS = [("ROBI", "রবিবার", "Sunday"), ("SOM", "সোমবার", "Monday"), ("MANGAL", "মঙ্গলবার", "Tuesday"),
        ("BUDH", "বুধবার", "Wednesday"), ("BRI", "বৃহস্পতিবার", "Thursday"), ("SUKRA", "শুক্রবার", "Friday"),
        ("SHONI", "শনিবার", "Saturday")]

# ── Weather descriptions (13px) — in order of WMO code ──
WX = [("পরিষ্কার আকাশ", "Clear sky"),
      ("প্রায় পরিষ্কার", "Mainly clear"),
      ("আংশিক মেঘলা", "Partly cloudy"),
      ("মেঘলা", "Overcast"),
      ("কুয়াশা", "Fog"),
      ("ঝিরঝিরে বৃষ্টি", "Drizzle"),
      ("ঠান্ডা গুঁড়ি বৃষ্টি", "Freezing drizzle"),
      ("হালকা বৃষ্টি", "Light rain"),
      ("বৃষ্টি", "Rain"),
      ("ভারী বৃষ্টি", "Heavy rain"),
      ("বরফ-বৃষ্টি", "Freezing rain"),
      ("তুষারপাত", "Snow fall"),
      ("হালকা বর্ষণ", "Light rain showers"),
      ("বর্ষণ", "Rain showers"),
      ("প্রবল বর্ষণ", "Heavy rain showers"),
      ("তুষার বর্ষণ", "Snow showers"),
      ("বজ্রসহ বৃষ্টি", "Thunderstorm"),
      ("শিলাবৃষ্টি", "Hailstorm"),
      ("জানা নেই", "Unknown")]

# ── Screen messages (12px) — exact same order as enum FaceMsg ──
MSGS = [("NONE",      "",                  ""),
        ("BOLUN",     "ছুঁয়ে ধরে বলুন",    "Touch and hold to speak"),
        ("SHUNCHHI",  "শুনছি...",          "Listening..."),
        ("BHABCHHI",  "ভাবছি...",          "Thinking..."),
        ("BOLCHHI",   "বলছি...",           "Speaking..."),
        ("JUKTECHHI", "জুড়ছি...",          "Connecting..."),
        ("KOTA",      "কোটা শেষ",          "Quota exceeded"),
        ("WIFI_NEI",  "ওয়াইফাই নেই",       "No Wi-Fi"),
        ("KEY_NEI",   "এপিআই কী নেই",      "No API key"),
        ("SEC_POR",   "সেকেন্ড পর",        "Seconds later"),
        ("SOMOSSA_M", "সমস্যা",            "Error"),
        ("OPEKKHA_M", "অপেক্ষা",           "Wait")]

# ── Day segments (11px) — instead of AM/PM. Appears next to the hour on the clock screen.
#    English AM/PM looks out of place on a Bengali clock, and Bengali segments are more descriptive.
PARTS = [("RAT",     "রাত",      "Night"),            # 0-3
         ("BHOR",    "ভোর",      "Dawn"),             # 4-5
         ("SOKAL",   "সকাল",     "Morning"),          # 6-11
         ("DUPUR",   "দুপুর",     "Noon/Afternoon"),   # 12-14
         ("BIKAL",   "বিকাল",     "Late afternoon"),   # 15-17
         ("SONDHYA", "সন্ধ্যা",    "Evening")]          # 18-19

# ── Large words / general labels (13-16px) ──
BIGWORDS = [("TITLE",     "স্টাডিমোচি",       16, "StudyMochi"),
            ("CHALU",     "চালু হচ্ছে...",     13, "Starting..."),
            ("SETUP",     "সেটআপ",            16, "Setup"),
            ("PHONE",     "ফোন দিয়ে জুড়ুন",    13, "Connect with phone"),
            ("BIROTI",    "বিরতি",             13, "Break"),
            ("GHORI_NEI", "ঘড়ি নেই",         13, "No clock"),
            ("WX_ANCHHI", "আবহাওয়া আনছি",    13, "Fetching weather"),
            ("ARDROTA",   "আর্দ্রতা",           12, "Humidity"),
            ("CHOLCHHE",  "চলছে",             12, "Running"),
            ("CHEPE",     "ছুঁয়ে ধরুন",         12, "Touch and hold"),
            ("TIMER",     "টাইমার",            13, "Timer"),
            ("BOSAN",     "সময় বসান",          12, "Set time"),
            ("SHESH",     "সময় শেষ",           13, "Time up"),
            ("THAMANO",   "থামানো",            12, "Paused"),
            ("MINIT",     "মিনিট",             12, "Minutes")]


def render(txt, size, H, baseline):
    if not txt:
        return 1, Image.new("1", (1, H), 0)
    f = ImageFont.truetype(FONT, size)
    im = Image.new("1", (460, H), 0)
    ImageDraw.Draw(im).text((14, baseline), txt, font=f, fill=1, anchor="ls")
    bb = im.getbbox()
    if not bb:
        return 1, im.crop((0, 0, 1, H))
    return bb[2] - bb[0], im.crop((bb[0], 0, bb[2], H))


def rows(im, w, H):
    """Adafruit drawBitmap: row-major, MSB first, row byte-aligned"""
    px = im.load()
    bpr = (w + 7) // 8
    out = []
    for y in range(H):
        for b in range(bpr):
            v = 0
            for bit in range(8):
                x = b * 8 + bit
                if x < w and px[x, y]:
                    v |= 0x80 >> bit
            out.append(v)
    return out, bpr


L = []
flash = [0]


def emit(name, txt, size, H, base, comment=True, label=None):
    w, im = render(txt, size, H, base)
    data, bpr = rows(im, w, H)
    flash[0] += len(data)
    if comment and txt:
        desc = label if label else txt
        L.append(f"// {desc}  ({w}x{H})")
    L.append(f"static const uint8_t {name}[] PROGMEM = {{")
    for y in range(H):
        L.append("  " + ",".join(f"0x{v:02X}" for v in data[y * bpr:(y + 1) * bpr]) + ",")
    L.append("};")
    return w


L.append('''// ════════════════════════════════════════════════════════════════
//   Bengali text — all bitmaps. U8g2 is not used.
//
//   ⚠️ This file is not handwritten. Generated by running gen_bangla.py.
//      If new text is needed, add to that script's list and re-run.
//
//   Why bitmaps:
//   U8g2's unifont cannot properly render Bengali conjuncts/ligatures.
//   Complex words like days, weather, humidity break into separate glyphs.
//   Pre-rendering them as bitmaps eliminates rendering issues and avoids
//   requiring Adafruit_GFX and U8g2_for_Adafruit_GFX libraries.
//
//   Font  : Noto Sans Bengali Bold
//   Format: Adafruit drawBitmap — row-major, MSB first, row byte-aligned.
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
''')

# ── Day names ──
L.append("// ───────────────── Day names ─────────────────")
L.append("#define BN_DAY_H 20\n")
dw = [emit(f"BN_DAY_{k}", t, 14, 20, 15, label=en) for k, t, en in DAYS]
L.append("static const uint8_t *const BN_DAY[7] PROGMEM = { " +
         ", ".join(f"BN_DAY_{k}" for k, _, _ in DAYS) + " };")
L.append("static const uint8_t BN_DAY_W[7] = { " + ", ".join(map(str, dw)) + " };\n")

# ── Large numbers ──
L.append("// ───────────────── Large numbers (Clock, Pomodoro) ─────────────────")
L.append("#define BN_BIG_H 22\n")
bw = [emit(f"BN_B{i}", ch, 20, 22, 17, False) for i, ch in enumerate("০১২৩৪৫৬৭৮৯")]
cw = emit("BN_BCOLON", ":", 20, 22, 17, False)
L.append("static const uint8_t *const BN_BIG[10] PROGMEM = { " +
         ", ".join(f"BN_B{i}" for i in range(10)) + " };")
L.append("static const uint8_t BN_BIG_W[10] = { " + ", ".join(map(str, bw)) + " };")
L.append(f"#define BN_BCOLON_W {cw}\n")

# ── Small numbers ──
L.append("// ───────────────── Small numbers (Date, Seconds) ─────────────────")
L.append("#define BN_NUM_H 12\n")
nw = [emit(f"BN_N{i}", ch, 11, 12, 9, False) for i, ch in enumerate("০১২৩৪৫৬৭৮৯")]
L.append("static const uint8_t *const BN_NUM[10] PROGMEM = { " +
         ", ".join(f"BN_N{i}" for i in range(10)) + " };")
L.append("static const uint8_t BN_NUM_W[10] = { " + ", ".join(map(str, nw)) + " };\n")

# ── Weather ──
L.append("// ───────────────── Weather conditions ─────────────────")
L.append("#define BN_WX_H 18")
L.append(f"#define BN_WX_N {len(WX)}\n")
ww = [emit(f"BN_WX_{i}", t, 13, 18, 13, label=en) for i, (t, en) in enumerate(WX)]
L.append(f"static const uint8_t *const BN_WX[{len(WX)}] PROGMEM = {{")
L.append("  " + ", ".join(f"BN_WX_{i}" for i in range(len(WX))) + "\n};")
L.append(f"static const uint8_t BN_WX_W[{len(WX)}] = {{ " + ", ".join(map(str, ww)) + " };\n")

# ── Screen messages ──
L.append("// ───────────────── Bottom line messages ─────────────────")
L.append("// ⚠️ Order must match enum FaceMsg in face.h exactly")
L.append("#define BN_MSG_H 16")
L.append(f"#define BN_MSG_N {len(MSGS)}\n")
mw = [emit(f"BN_MSG_{k}", t, 12, 16, 12, label=en) for k, t, en in MSGS]
L.append(f"static const uint8_t *const BN_MSG[{len(MSGS)}] PROGMEM = {{")
L.append("  " + ", ".join(f"BN_MSG_{k}" for k, _, _ in MSGS) + "\n};")
L.append(f"static const uint8_t BN_MSG_W[{len(MSGS)}] = {{ " + ", ".join(map(str, mw)) + " };\n")

# ── Day segments ──
L.append("// ───────────────── Day segments (not AM/PM) ─────────────────")
L.append("#define BN_PART_H 15")
L.append(f"#define BN_PART_N {len(PARTS)}\n")
pw = [emit(f"BN_PART_{k}", t, 11, 15, 11, label=en) for k, t, en in PARTS]
L.append(f"static const uint8_t *const BN_PART[{len(PARTS)}] PROGMEM = {{")
L.append("  " + ", ".join(f"BN_PART_{k}" for k, _, _ in PARTS) + "\n};")
L.append(f"static const uint8_t BN_PART_W[{len(PARTS)}] = {{ " + ", ".join(map(str, pw)) + " };\n")

# ── Large words / other text ──
L.append("// ───────────────── Other text ─────────────────")
for key, txt, size, en in BIGWORDS:
    H = size + 6
    base = size + 1
    w = emit(f"BN_{key}", txt, size, H, base, label=en)
    L.append(f"#define BN_{key}_W {w}")
    L.append(f"#define BN_{key}_H {H}\n")

open(OUT, "w", encoding="utf-8").write("\n".join(L) + "\n")
print(f"day  : {dw}")
print(f"big  : {bw}  colon {cw}")
print(f"num  : {nw}")
print(f"wx   : {ww}")
print(f"msg  : {mw}")
print(f"part : {pw}")
print(f"flash: {flash[0]} byte")

