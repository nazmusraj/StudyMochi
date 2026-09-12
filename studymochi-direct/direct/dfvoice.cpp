#include "dfvoice.h"
#include <HardwareSerial.h>

static HardwareSerial gDfSerial(2); // UART2 on ESP32
static bool gDfInit = false;
static uint32_t gLastPlayTime = 0;
static uint32_t gCurrentCooldown = 800;

// DFPlayer standard command packet (10 bytes)
static void sendDfCmd(uint8_t cmd, uint8_t param1, uint8_t param2) {
  if (!gDfInit) return;

  uint16_t checksum = -(0xFF + 0x06 + cmd + 0x00 + param1 + param2);
  uint8_t pkt[10] = {
    0x7E,             // Start
    0xFF,             // Version
    0x06,             // Length
    cmd,              // Command
    0x00,             // Feedback (0 = no ack)
    param1,           // Param high
    param2,           // Param low
    (uint8_t)(checksum >> 8),   // Checksum high
    (uint8_t)(checksum & 0xFF), // Checksum low
    0xEF              // End
  };

  gDfSerial.write(pkt, 10);
  gDfSerial.flush();
}

void dfvoiceBegin(int rxPin, int txPin, uint8_t volume) {
  if (rxPin < 0 || txPin < 0) return;
  gDfSerial.begin(9600, SERIAL_8N1, rxPin, txPin);
  delay(100);
  gDfInit = true;

  dfvoiceSetVolume(volume);
  delay(50);
}

void dfvoiceSetVolume(uint8_t volume) {
  if (volume > 30) volume = 30;
  sendDfCmd(0x06, 0x00, volume);
}

void dfvoicePlayNum(uint16_t trackNum, uint32_t cooldownMs) {
  if (!gDfInit || trackNum == 0) return;
  uint32_t now = millis();
  if (now - gLastPlayTime < gCurrentCooldown) return;

  gLastPlayTime = now;
  gCurrentCooldown = cooldownMs;

  // 0x03 = Play track by index from root or /mp3 folder
  sendDfCmd(0x03, (uint8_t)(trackNum >> 8), (uint8_t)(trackNum & 0xFF));
}

void dfvoicePlay(BanglaVoiceTrack track, uint32_t cooldownMs) {
  dfvoicePlayNum((uint16_t)track, cooldownMs);
}

void dfvoiceStop() {
  sendDfCmd(0x16, 0x00, 0x00);
}

bool dfvoiceIsAvailable() {
  return gDfInit;
}
