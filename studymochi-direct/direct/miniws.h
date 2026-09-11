// ════════════════════════════════════════════════════════════════
//   MiniWS — Lightweight WebSocket client designed for streaming reads.
//
//   Why write our own:
//   Standard libraries (e.g. arduinoWebSockets) buffer entire frames
//   in RAM before yielding data. Gemini response audio frames can be
//   30-50 KB — on ESP32 without PSRAM, there is insufficient memory.
//
//   Here we parse the frame header and stream the payload byte-by-byte.
//   This allows handling 100+ KB frames using only a few hundred bytes of RAM.
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

class MiniWS {
public:
  // Connects via TLS and performs the WebSocket handshake.
  // extraHeader, if provided, is sent verbatim as a header line
  // (e.g. "x-goog-api-key: ...") — CRLF is appended automatically.
  bool connect(const char *host, uint16_t port, const char *path,
               const char *extraHeader = nullptr);
  void stop();
  bool connected();

  // ── Transmission ──
  // Sends a text frame. Large JSON payloads are supported.
  bool sendText(const char *data, size_t len);
  bool sendText(const String &s) { return sendText(s.c_str(), s.length()); }

  // ── Reception ──
  // Returns true when a new frame begins. Read the payload with readByte().
  // opcode: 1=text, 2=binary, 8=close, 9=ping, 10=pong
  bool beginFrame(uint8_t &opcode, uint64_t &length, uint32_t timeoutMs = 20);

  // Next payload byte. Returns -1 on EOF or error.
  int readByte();

  // Reads multiple bytes in bulk — much faster than byte-by-byte.
  // Returns the actual number of bytes read.
  size_t readInto(uint8_t *dst, size_t n);

  // Discards any remaining payload bytes and concludes the current frame.
  void endFrame();

  // Responds automatically to ping with pong — call inside loop().
  void handleControl(uint8_t opcode, uint64_t length);

  // Parses the closure reason code and message from a close frame (opcode 8).
  // Explains why the server disconnected.
  void readClose(uint64_t length, uint16_t &code, char *reason, size_t reasonSz);

private:
  WiFiClientSecure _c;
  uint64_t _remain = 0;          // Remaining payload bytes in current frame
  bool     _inFrame = false;

  // ── Read Buffer ──
  // Invoking mbedtls per-byte is extremely slow. We fetch 512 bytes
  // at once into this buffer and serve reads from here.
  uint8_t _rb[512];
  size_t  _rbLen = 0, _rbPos = 0;

  bool   fillRb(uint32_t timeoutMs);
  bool   hasByte();
  int    rawByte(uint32_t timeoutMs);
  bool   rawExact(uint8_t *dst, size_t n, uint32_t timeoutMs);

  bool writeAll(const uint8_t *d, size_t n);
  bool writeFrame(uint8_t opcode, const uint8_t *data, size_t len);
};
