// ════════════════════════════════════════════════════════════════
//   MiniWS — ছোট্ট WebSocket ক্লায়েন্ট, স্ট্রিমিং পড়ার জন্য।
//
//   কেন নিজেরা লিখছি:
//   সাধারণ লাইব্রেরি (arduinoWebSockets) পুরো ফ্রেমটা RAM-এ জমা
//   করে তারপর হাতে দেয়। Gemini-র উত্তরের অডিও ফ্রেম ৩০-৫০ KB-ও
//   হতে পারে — PSRAM ছাড়া ESP32-তে সেটা রাখার জায়গা নেই।
//
//   এখানে ফ্রেমের হেডার পড়ে **পেলোড বাইট-বাই-বাইট** হাতে দেওয়া হয়।
//   তাই ১০০ KB-র ফ্রেমও মাত্র কয়েকশো বাইট RAM-এ সামলানো যায়।
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

class MiniWS {
public:
  // TLS দিয়ে যুক্ত হয়ে WebSocket হ্যান্ডশেক করে।
  // extraHeader দিলে সেটা হুবহু একটা হেডার লাইন হিসেবে যায়
  // (যেমন "x-goog-api-key: ...") — CRLF আমরা বসাব।
  bool connect(const char *host, uint16_t port, const char *path,
               const char *extraHeader = nullptr);
  void stop();
  bool connected();

  // ── পাঠানো ──
  // টেক্সট ফ্রেম। বড় JSON-ও পাঠানো যায়।
  bool sendText(const char *data, size_t len);
  bool sendText(const String &s) { return sendText(s.c_str(), s.length()); }

  // ── গ্রহণ ──
  // নতুন একটা ফ্রেম এলে true. তারপর readByte() দিয়ে পেলোড পড়ুন।
  // opcode: 1=text, 2=binary, 8=close, 9=ping, 10=pong
  bool beginFrame(uint8_t &opcode, uint64_t &length, uint32_t timeoutMs = 20);

  // পেলোডের পরের বাইট। শেষ হলে -1।
  int readByte();

  // একসাথে অনেকগুলো বাইট — বাইট-বাই-বাইটের চেয়ে অনেক দ্রুত।
  // কত বাইট আসলে পড়া গেল সেটা ফেরত দেয়।
  size_t readInto(uint8_t *dst, size_t n);

  // বাকি পেলোড ফেলে দিয়ে ফ্রেম শেষ করে
  void endFrame();

  // ping এলে নিজে থেকে pong পাঠায় — loop()-এ ডাকুন
  void handleControl(uint8_t opcode, uint64_t length);

  // close (opcode 8) ফ্রেমের ভেতরের কারণটা পড়ে।
  // সার্ভার কেন লাইন কেটে দিল — এটাই বলে দেয়।
  void readClose(uint64_t length, uint16_t &code, char *reason, size_t reasonSz);

private:
  WiFiClientSecure _c;
  uint64_t _remain = 0;          // এই ফ্রেমে আর কত বাইট বাকি
  bool     _inFrame = false;

  // ── পড়ার বাফার ──
  // প্রতি বাইটে একবার করে mbedtls-এ ঢোকা খুব ধীর। তাই ৫১২ বাইট
  // একসাথে টেনে এনে এখান থেকে বিলি করি।
  uint8_t _rb[512];
  size_t  _rbLen = 0, _rbPos = 0;

  bool   fillRb(uint32_t timeoutMs);
  bool   hasByte();
  int    rawByte(uint32_t timeoutMs);
  bool   rawExact(uint8_t *dst, size_t n, uint32_t timeoutMs);

  bool writeAll(const uint8_t *d, size_t n);
  bool writeFrame(uint8_t opcode, const uint8_t *data, size_t len);
};
