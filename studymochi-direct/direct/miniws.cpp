#include "miniws.h"
#include <mbedtls/base64.h>
#include <esp_random.h>

// ───────────────────────── হ্যান্ডশেক ─────────────────────────
bool MiniWS::connect(const char *host, uint16_t port, const char *path,
                     const char *extraHeader) {
  stop();

  // ⚠️ setInsecure() মানে সার্ভারের সার্টিফিকেট যাচাই করা হচ্ছে না।
  // ট্রাফিক এনক্রিপ্টেড থাকে (কেউ আড়ি পেতে পড়তে পারবে না), কিন্তু
  // সক্রিয় MITM আক্রমণ ঠেকানো যায় না। নিজের WiFi-তে ব্যক্তিগত
  // ডিভাইসের জন্য এটা স্বাভাবিক পছন্দ। বেশি নিরাপত্তা চাইলে
  // Google-এর root CA বসিয়ে _c.setCACert(...) ব্যবহার করুন।
  _c.setInsecure();
  _c.setTimeout(15);

  Serial.printf("[ws] TLS juktechi %s:%u ...\n", host, port);
  if (!_c.connect(host, port)) {
    Serial.println("[ws] TLS connect BYARTHO");
    return false;
  }

  // ── Sec-WebSocket-Key: ১৬ র‍্যান্ডম বাইটের base64 ──
  uint8_t nonce[16];
  for (int i = 0; i < 16; i++) nonce[i] = (uint8_t)(esp_random() & 0xFF);
  unsigned char keyB64[32]; size_t keyLen = 0;
  mbedtls_base64_encode(keyB64, sizeof(keyB64), &keyLen, nonce, sizeof(nonce));
  keyB64[keyLen] = 0;

  // ── HTTP Upgrade ──
  _c.printf("GET %s HTTP/1.1\r\n", path);
  _c.printf("Host: %s\r\n", host);
  _c.print("Upgrade: websocket\r\n");
  _c.print("Connection: Upgrade\r\n");
  _c.printf("Sec-WebSocket-Key: %s\r\n", (char *)keyB64);
  _c.print("Sec-WebSocket-Version: 13\r\n");
  if (extraHeader && *extraHeader) _c.printf("%s\r\n", extraHeader);
  _c.print("\r\n");

  // ── উত্তর: 101 Switching Protocols চাই ──
  uint32_t t0 = millis();
  bool ok101 = false, headersDone = false;
  String line;
  while (millis() - t0 < 15000 && !headersDone) {
    while (_c.available()) {
      char ch = (char)_c.read();
      if (ch == '\n') {
        line.trim();
        if (line.length() == 0) { headersDone = true; break; }
        if (line.startsWith("HTTP/1.1 101") || line.startsWith("HTTP/1.0 101"))
          ok101 = true;
        else if (line.startsWith("HTTP/"))
          Serial.printf("[ws] server bollo: %s\n", line.c_str());
        line = "";
      } else if (ch != '\r') {
        if (line.length() < 200) line += ch;
      }
    }
    if (!headersDone) delay(5);
  }

  if (!ok101) {
    Serial.println("[ws] handshake BYARTHO (101 pai ni)");
    stop();
    return false;
  }
  Serial.println("[ws] handshake OK");
  return true;
}

void MiniWS::stop() {
  if (_c.connected()) _c.stop();
  _remain = 0;
  _inFrame = false;
  _rbLen = _rbPos = 0;          // বাফারে পুরোনো বাইট রেখে দেব না
}

bool MiniWS::connected() { return _c.connected(); }

// ───────────────────────── পাঠানো ─────────────────────────
// _c.write() চাওয়ার চেয়ে কম বাইটও লিখতে পারে। আগে সেটা হলে আমরা
// মাঝপথে হাল ছেড়ে দিতাম — ফলে অর্ধেক ফ্রেম চলে যেত আর সার্ভার
// আবোল-তাবোল পেয়ে লাইন কেটে দিত। এখন শেষ না হওয়া পর্যন্ত চেষ্টা করি।
bool MiniWS::writeAll(const uint8_t *d, size_t n) {
  size_t sent = 0;
  uint32_t t0 = millis();
  while (sent < n) {
    if (!_c.connected()) return false;
    size_t w = _c.write(d + sent, n - sent);
    if (w > 0) { sent += w; t0 = millis(); continue; }
    if (millis() - t0 > 5000) return false;      // ৫ সেকেন্ড ধরে কিছুই গেল না
    delay(1);
  }
  return true;
}

// ক্লায়েন্ট → সার্ভার ফ্রেম **অবশ্যই** মাস্ক করতে হয় (RFC 6455)
bool MiniWS::writeFrame(uint8_t opcode, const uint8_t *data, size_t len) {
  if (!_c.connected()) return false;

  uint8_t hdr[14];
  size_t h = 0;
  hdr[h++] = 0x80 | opcode;                   // FIN + opcode

  if (len < 126) {
    hdr[h++] = 0x80 | (uint8_t)len;           // MASK + len
  } else if (len < 65536) {
    hdr[h++] = 0x80 | 126;
    hdr[h++] = (uint8_t)(len >> 8);
    hdr[h++] = (uint8_t)(len & 0xFF);
  } else {
    hdr[h++] = 0x80 | 127;
    for (int i = 7; i >= 0; i--) hdr[h++] = (uint8_t)((uint64_t)len >> (8 * i));
  }

  uint8_t mask[4];
  for (int i = 0; i < 4; i++) { mask[i] = (uint8_t)(esp_random() & 0xFF); hdr[h++] = mask[i]; }

  if (!writeAll(hdr, h)) {
    Serial.println("[ws] header pathate parlam na — line bondho korchi");
    stop();
    return false;
  }

  // মাস্ক করে টুকরো টুকরো পাঠাই — বড় বাফার লাগে না
  uint8_t buf[512];
  size_t sent = 0;
  while (sent < len) {
    size_t n = len - sent; if (n > sizeof(buf)) n = sizeof(buf);
    for (size_t i = 0; i < n; i++)
      buf[i] = data[sent + i] ^ mask[(sent + i) & 3];
    if (!writeAll(buf, n)) {
      // অর্ধেক ফ্রেম চলে গেছে — এই লাইন আর বিশ্বাস করা যায় না
      Serial.println("[ws] payload pathate parlam na — line bondho korchi");
      stop();
      return false;
    }
    sent += n;
  }
  return true;
}

bool MiniWS::sendText(const char *data, size_t len) {
  return writeFrame(0x1, (const uint8_t *)data, len);
}

// ───────────────────────── গ্রহণ ─────────────────────────
bool MiniWS::fillRb(uint32_t timeoutMs) {
  if (_rbPos < _rbLen) return true;
  _rbPos = _rbLen = 0;
  uint32_t t0 = millis();
  while (true) {
    if (_c.available() > 0) {
      int r = _c.read(_rb, sizeof(_rb));
      if (r > 0) { _rbLen = (size_t)r; return true; }
    }
    if (!_c.connected() && _c.available() <= 0) return false;
    if (millis() - t0 >= timeoutMs) return false;
    delay(1);
  }
}

bool MiniWS::hasByte() { return _rbPos < _rbLen || _c.available() > 0; }

int MiniWS::rawByte(uint32_t timeoutMs) {
  if (_rbPos >= _rbLen && !fillRb(timeoutMs)) return -1;
  return _rb[_rbPos++];
}

bool MiniWS::rawExact(uint8_t *dst, size_t n, uint32_t timeoutMs) {
  size_t got = 0;
  while (got < n) {
    if (_rbPos >= _rbLen && !fillRb(timeoutMs)) return false;
    size_t avail = _rbLen - _rbPos;
    size_t take  = n - got; if (take > avail) take = avail;
    memcpy(dst + got, _rb + _rbPos, take);
    _rbPos += take; got += take;
  }
  return true;
}

bool MiniWS::beginFrame(uint8_t &opcode, uint64_t &length, uint32_t timeoutMs) {
  if (_inFrame) endFrame();
  if (!hasByte()) return false;

  uint8_t b[2];
  if (!rawExact(b, 2, timeoutMs + 200)) return false;

  opcode = b[0] & 0x0F;
  bool masked = (b[1] & 0x80) != 0;           // সার্ভার → ক্লায়েন্ট মাস্ক করে না
  uint64_t len = b[1] & 0x7F;

  if (len == 126) {
    uint8_t e[2];
    if (!rawExact(e, 2, 2000)) return false;
    len = ((uint64_t)e[0] << 8) | e[1];
  } else if (len == 127) {
    uint8_t e[8];
    if (!rawExact(e, 8, 2000)) return false;
    len = 0;
    for (int i = 0; i < 8; i++) len = (len << 8) | e[i];
  }

  if (masked) {                                // নিয়ম ভাঙলে বাদ দিই
    uint8_t m[4];
    if (!rawExact(m, 4, 2000)) return false;
  }

  length = len;
  _remain = len;
  _inFrame = true;
  return true;
}

int MiniWS::readByte() {
  if (!_inFrame || _remain == 0) return -1;
  int v = rawByte(5000);
  if (v < 0) return -1;
  _remain--;
  return v;
}

size_t MiniWS::readInto(uint8_t *dst, size_t n) {
  if (!_inFrame) return 0;
  if ((uint64_t)n > _remain) n = (size_t)_remain;
  size_t got = 0;
  while (got < n) {
    if (_rbPos >= _rbLen && !fillRb(5000)) break;
    size_t avail = _rbLen - _rbPos;
    size_t take  = n - got; if (take > avail) take = avail;
    memcpy(dst + got, _rb + _rbPos, take);
    _rbPos += take; got += take; _remain -= take;
  }
  return got;
}

void MiniWS::endFrame() {
  uint8_t sink[128];
  while (_remain > 0) {
    size_t n = _remain > sizeof(sink) ? sizeof(sink) : (size_t)_remain;
    if (readInto(sink, n) == 0) break;
  }
  _remain = 0;
  _inFrame = false;
}

void MiniWS::handleControl(uint8_t opcode, uint64_t length) {
  if (opcode == 0x9) {                         // ping → pong
    uint8_t body[125];
    size_t n = length > sizeof(body) ? sizeof(body) : (size_t)length;
    n = readInto(body, n);
    endFrame();
    writeFrame(0xA, body, n);
  } else {
    endFrame();                                // pong / অন্য কিছু — ফেলে দিই
  }
}

// close ফ্রেম: প্রথম ২ বাইট স্ট্যাটাস কোড, বাকিটা UTF-8 কারণ।
// আগে এটা ফেলে দিতাম — তাই "server bondho korlo" ছাড়া কিছু জানতাম না।
void MiniWS::readClose(uint64_t length, uint16_t &code, char *reason, size_t reasonSz) {
  code = 0;
  if (reasonSz) reason[0] = 0;
  size_t idx = 0;

  if (length >= 2) {
    uint8_t b[2];
    if (readInto(b, 2) == 2) code = (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
  }
  while (_remain > 0) {
    int v = readByte();
    if (v < 0) break;
    if (reasonSz && idx + 1 < reasonSz) reason[idx++] = (char)v;
  }
  if (reasonSz) reason[idx] = 0;
  endFrame();
}
