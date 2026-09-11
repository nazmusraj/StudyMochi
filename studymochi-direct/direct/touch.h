// ════════════════════════════════════════════════════════════════
//   TTP223 টাচ সেন্সর — ছোঁয়া, ট্যাপ আর চেপে ধরা আলাদা করে চেনে।
//
//   ওয়্যারিং (প্রতিটা মডিউলে ৩টা পিন):
//       VCC → 3V3      GND → GND      OUT/SIG → GPIO
//
//   TTP223 ছুঁলে OUT **HIGH** হয় (ডিফল্ট সেটিং)। কিছু মডিউলে
//   পেছনে A/B প্যাড জোড়া দিয়ে উল্টো করা যায় — উল্টো হলে
//   TOUCH_ACTIVE_LOW 1 করে দিন।
//
//   কেন আলাদা ফাইল: বাটনের হিসাব (ঝাঁকুনি সরানো, ট্যাপ বনাম
//   চেপে ধরা) এক জায়গায় থাকলে direct.ino পরিষ্কার থাকে, আর
//   পিসিতে টেস্টও করা যায়।
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

// মডিউল উল্টো হলে ১ করুন
#ifndef TOUCH_ACTIVE_LOW
#define TOUCH_ACTIVE_LOW 0
#endif

#define TOUCH_DEBOUNCE_MS 40      // ঝাঁকুনি সরাতে
#define TOUCH_HOLD_MS     600     // এর বেশি ধরলে "চেপে ধরা"

class Touch {
public:
  void begin(int pin);

  // loop()-এ বারবার ডাকুন। এখনকার সময়টা বাইরে থেকে দিই যাতে
  // পিসিতে টেস্ট করা যায় (millis() নকল করা যায়)।
  void update(uint32_t now);

  bool isDown() const { return _stable; }          // এখন ছোঁয়া আছে কি
  uint32_t heldMs(uint32_t now) const;             // কতক্ষণ ধরে

  // ── ঘটনা: একবারই true দেয়, পড়ে নিলেই মুছে যায় ──
  bool tookTap();        // ছোট ছোঁয়া (ছেড়ে দেওয়ার পর)
  bool tookHold();       // চেপে ধরা শুরু হলো (ছাড়ার অপেক্ষা নয়)
  bool tookRelease();    // ছেড়ে দিল

private:
  int      _pin      = -1;
  bool     _raw      = false;
  bool     _stable   = false;
  uint32_t _changed  = 0;
  uint32_t _downAt   = 0;
  bool     _holdSent = false;
  bool     _tap = false, _hold = false, _rel = false;
};
