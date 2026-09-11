#!/usr/bin/env python3
"""
banglabmp.h বানায় — StudyMochi-র সব বাংলা লেখা, আগেই ছবি করে রাখা।

কেন এই স্ক্রিপ্ট: U8g2-র unifont বাংলা যুক্তাক্ষর জোড়া দিতে পারে না
("পড়ার সময়", "বৃহস্পতিবার", "আর্দ্রতা" — সব ভেঙে যায়)। তাই লেখাগুলো
Noto Sans Bengali দিয়ে আগেই render করে PROGMEM বিটম্যাপ বানিয়ে রাখি।

নতুন লেখা লাগলে নিচের তালিকায় যোগ করে আবার চালালেই হবে:
    python3 gen_bangla.py > /dev/null
"""
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/noto/NotoSansBengali-Bold.ttf"
OUT = "/home/claude/direct/banglabmp.h"

# ── বারের নাম (১৪px) ──
DAYS = [("ROBI", "রবিবার"), ("SOM", "সোমবার"), ("MANGAL", "মঙ্গলবার"),
        ("BUDH", "বুধবার"), ("BRI", "বৃহস্পতিবার"), ("SUKRA", "শুক্রবার"),
        ("SHONI", "শনিবার")]

# ── আবহাওয়ার কথা (১৩px) — WMO কোডের ক্রমে ──
WX = ["পরিষ্কার আকাশ", "প্রায় পরিষ্কার", "আংশিক মেঘলা", "মেঘলা", "কুয়াশা",
      "ঝিরঝিরে বৃষ্টি", "ঠান্ডা গুঁড়ি বৃষ্টি", "হালকা বৃষ্টি", "বৃষ্টি",
      "ভারী বৃষ্টি", "বরফ-বৃষ্টি", "তুষারপাত", "হালকা বর্ষণ", "বর্ষণ",
      "প্রবল বর্ষণ", "তুষার বর্ষণ", "বজ্রসহ বৃষ্টি", "শিলাবৃষ্টি", "জানা নেই"]

# ── পর্দার লেখা (১২px) — enum FaceMsg-এর হুবহু একই ক্রম ──
MSGS = [("NONE",      ""),
        ("BOLUN",     "ছুঁয়ে ধরে বলুন"),
        ("SHUNCHHI",  "শুনছি..."),
        ("BHABCHHI",  "ভাবছি..."),
        ("BOLCHHI",   "বলছি..."),
        ("JUKTECHHI", "জুড়ছি..."),
        ("KOTA",      "কোটা শেষ"),
        ("WIFI_NEI",  "ওয়াইফাই নেই"),
        ("KEY_NEI",   "এপিআই কী নেই"),
        ("SEC_POR",   "সেকেন্ড পর"),
        ("SOMOSSA_M", "সমস্যা"),
        ("OPEKKHA_M", "অপেক্ষা")]

# ── দিনের ভাগ (১১px) — AM/PM-এর বদলে। ঘড়ির পর্দায় ঘণ্টার পাশে বসে।
#    ইংরেজি AM/PM বাংলা ঘড়িতে বেমানান, আর বাংলায় ভাগটা বেশি স্পষ্ট।
PARTS = [("RAT",     "রাত"),      # ০-৩
         ("BHOR",    "ভোর"),      # ৪-৫
         ("SOKAL",   "সকাল"),     # ৬-১১
         ("DUPUR",   "দুপুর"),     # ১২-১৪
         ("BIKAL",   "বিকাল"),     # ১৫-১৭
         ("SONDHYA", "সন্ধ্যা")]    # ১৮-১৯

# ── বড় লেখা (১৩-১৬px) ──
BIGWORDS = [("TITLE",   "স্টাডিমোচি",    16),
            ("CHALU",   "চালু হচ্ছে...",  13),
            ("SETUP",   "সেটআপ",         16),
            ("PHONE",   "ফোন দিয়ে জুড়ুন", 13),
            ("BIROTI",  "বিরতি",          13),
            ("GHORI_NEI", "ঘড়ি নেই",      13),
            ("WX_ANCHHI", "আবহাওয়া আনছি", 13),
            ("ARDROTA", "আর্দ্রতা",        12),
            ("CHOLCHHE", "চলছে",          12),
            ("CHEPE",   "ছুঁয়ে ধরুন",      12),
            ("TIMER",   "টাইমার",         13),
            ("BOSAN",   "সময় বসান",       12),
            ("SHESH",   "সময় শেষ",        13),
            ("THAMANO", "থামানো",         12),
            ("MINIT",   "মিনিট",          12)]


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
    """Adafruit drawBitmap: row-major, MSB আগে, সারি বাইটে গোল"""
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


def emit(name, txt, size, H, base, comment=True):
    w, im = render(txt, size, H, base)
    data, bpr = rows(im, w, H)
    flash[0] += len(data)
    if comment and txt:
        L.append(f"// {txt}  ({w}x{H})")
    L.append(f"static const uint8_t {name}[] PROGMEM = {{")
    for y in range(H):
        L.append("  " + ",".join(f"0x{v:02X}" for v in data[y * bpr:(y + 1) * bpr]) + ",")
    L.append("};")
    return w


L.append('''// ════════════════════════════════════════════════════════════════
//   বাংলা লেখা — সব বিটম্যাপ। U8g2 ব্যবহার করা হয়নি।
//
//   ⚠️ এই ফাইলটা হাতে লেখা নয়। gen_bangla.py চালিয়ে তৈরি।
//      নতুন লেখা লাগলে ওই স্ক্রিপ্টের তালিকায় যোগ করে আবার চালান।
//
//   কেন বিটম্যাপ:
//   U8g2-র unifont বাংলা যুক্তাক্ষর জোড়া দিতে পারে না। "পড়ার সময়",
//   "বৃহস্পতিবার", "আর্দ্রতা", "বজ্রসহ বৃষ্টি" — সবই ভেঙে যায়। আগেই
//   ছবি করে রাখলে ভাঙার সম্ভাবনা শূন্য, আর Adafruit_GFX ও
//   U8g2_for_Adafruit_GFX — দুটো লাইব্রেরিই বাদ পড়ে।
//
//   তৈরি: Noto Sans Bengali Bold
//   ধরন : Adafruit drawBitmap — row-major, MSB আগে, সারি বাইটে গোল।
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
''')

# ── barer nam ──
L.append("// ───────────────── বারের নাম ─────────────────")
L.append("#define BN_DAY_H 20\n")
dw = [emit(f"BN_DAY_{k}", t, 14, 20, 15) for k, t in DAYS]
L.append("static const uint8_t *const BN_DAY[7] PROGMEM = { " +
         ", ".join(f"BN_DAY_{k}" for k, _ in DAYS) + " };")
L.append("static const uint8_t BN_DAY_W[7] = { " + ", ".join(map(str, dw)) + " };\n")

# ── boro shonkha ──
L.append("// ───────────────── বড় সংখ্যা (ঘড়ি, পমোডোরো) ─────────────────")
L.append("#define BN_BIG_H 22\n")
bw = [emit(f"BN_B{i}", ch, 20, 22, 17, False) for i, ch in enumerate("০১২৩৪৫৬৭৮৯")]
cw = emit("BN_BCOLON", ":", 20, 22, 17, False)
L.append("static const uint8_t *const BN_BIG[10] PROGMEM = { " +
         ", ".join(f"BN_B{i}" for i in range(10)) + " };")
L.append("static const uint8_t BN_BIG_W[10] = { " + ", ".join(map(str, bw)) + " };")
L.append(f"#define BN_BCOLON_W {cw}\n")

# ── chhoto shonkha ──
L.append("// ───────────────── ছোট সংখ্যা (তারিখ, সেকেন্ড) ─────────────────")
L.append("#define BN_NUM_H 12\n")
nw = [emit(f"BN_N{i}", ch, 11, 12, 9, False) for i, ch in enumerate("০১২৩৪৫৬৭৮৯")]
L.append("static const uint8_t *const BN_NUM[10] PROGMEM = { " +
         ", ".join(f"BN_N{i}" for i in range(10)) + " };")
L.append("static const uint8_t BN_NUM_W[10] = { " + ", ".join(map(str, nw)) + " };\n")

# ── abohawa ──
L.append("// ───────────────── আবহাওয়ার কথা ─────────────────")
L.append("#define BN_WX_H 18")
L.append(f"#define BN_WX_N {len(WX)}\n")
ww = [emit(f"BN_WX_{i}", t, 13, 18, 13) for i, t in enumerate(WX)]
L.append(f"static const uint8_t *const BN_WX[{len(WX)}] PROGMEM = {{")
L.append("  " + ", ".join(f"BN_WX_{i}" for i in range(len(WX))) + "\n};")
L.append(f"static const uint8_t BN_WX_W[{len(WX)}] = {{ " + ", ".join(map(str, ww)) + " };\n")

# ── porda-r lekha ──
L.append("// ───────────────── নিচের লাইনের লেখা ─────────────────")
L.append("// ⚠️ ক্রমটা face.h-এর enum FaceMsg-এর সাথে হুবহু মিলতে হবে")
L.append("#define BN_MSG_H 16")
L.append(f"#define BN_MSG_N {len(MSGS)}\n")
mw = [emit(f"BN_MSG_{k}", t, 12, 16, 12) for k, t in MSGS]
L.append(f"static const uint8_t *const BN_MSG[{len(MSGS)}] PROGMEM = {{")
L.append("  " + ", ".join(f"BN_MSG_{k}" for k, _ in MSGS) + "\n};")
L.append(f"static const uint8_t BN_MSG_W[{len(MSGS)}] = {{ " + ", ".join(map(str, mw)) + " };\n")

# ── diner bhag ──
L.append("// ───────────────── দিনের ভাগ (AM/PM নয়) ─────────────────")
L.append("#define BN_PART_H 15")
L.append(f"#define BN_PART_N {len(PARTS)}\n")
pw = [emit(f"BN_PART_{k}", t, 11, 15, 11) for k, t in PARTS]
L.append(f"static const uint8_t *const BN_PART[{len(PARTS)}] PROGMEM = {{")
L.append("  " + ", ".join(f"BN_PART_{k}" for k, _ in PARTS) + "\n};")
L.append(f"static const uint8_t BN_PART_W[{len(PARTS)}] = {{ " + ", ".join(map(str, pw)) + " };\n")

# ── boro kotha ──
L.append("// ───────────────── অন্য লেখা ─────────────────")
for key, txt, size in BIGWORDS:
    H = size + 6
    base = size + 1
    w = emit(f"BN_{key}", txt, size, H, base)
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
