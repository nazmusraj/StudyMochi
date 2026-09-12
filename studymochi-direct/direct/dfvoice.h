// ════════════════════════════════════════════════════════════════
//   DFPlayer Mini Bangla Voice Driver for StudyMochi
//
//   Uses ESP32 HardwareSerial2:
//       ESP32 GPIO 16 (RX2) ◄── DFPlayer TX
//       ESP32 GPIO 17 (TX2) ──► 1kΩ resistor ──► DFPlayer RX
//       5V / GND to power rails
//
//   MicroSD folder structure:
//     /mp3/0001.mp3  -> Happy cuddle ("খুব ভালো লাগছে! ভালোবাসি তোমাকে!")
//     /mp3/0002.mp3  -> Angry tap    ("উফ! মাথায় এভাবে মারছ কেন?")
//     /mp3/0003.mp3  -> Pomo start   ("পড়ার সময় শুরু! বইয়ে মন দাও।")
//     /mp3/0004.mp3  -> Pomo break   ("ব্রেক টাইম! একটু পানি খেয়ে নাও।")
//     /mp3/0005.mp3  -> Pomo finish  ("সাবাশ! আজকের সেশন সম্পন্ন হলো!")
//     /mp3/0006.mp3  -> Mode clock   ("ঘড়ি মোড")
//     /mp3/0007.mp3  -> Mode timer   ("টাইমার ও পোমোডোরো মোড")
//     /mp3/0008.mp3  -> Mode AI      ("এআই মোড চালু হয়েছে")
//     /mp3/0009.mp3  -> Squish left  ("আরে আরে! বামে কাত হয়ে গেলাম!")
//     /mp3/0010.mp3  -> Squish right ("ডানে কাত হয়ে চ্যাপ্টা হয়ে গেলাম!")
//     /mp3/0011.mp3  -> Shaken dizzy ("মাথা ঘুরছে! থামাও!")
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

enum BanglaVoiceTrack {
  VOICE_NONE          = 0,
  VOICE_HAPPY_CUDDLE  = 1,
  VOICE_ANGRY_TAP     = 2,
  VOICE_POMO_START    = 3,
  VOICE_POMO_BREAK    = 4,
  VOICE_POMO_FINISH   = 5,
  VOICE_MODE_CLOCK    = 6,
  VOICE_MODE_TIMER    = 7,
  VOICE_MODE_AI       = 8,
  VOICE_SQUISH_LEFT   = 9,
  VOICE_SQUISH_RIGHT  = 10,
  VOICE_SHAKEN_DIZZY  = 11
};

void dfvoiceBegin(int rxPin = 16, int txPin = 17, uint8_t volume = 24);
void dfvoicePlay(BanglaVoiceTrack track, uint32_t cooldownMs = 800);
void dfvoicePlayNum(uint16_t trackNum, uint32_t cooldownMs = 800);
void dfvoiceSetVolume(uint8_t volume); // 0-30
void dfvoiceStop();
bool dfvoiceIsAvailable();
