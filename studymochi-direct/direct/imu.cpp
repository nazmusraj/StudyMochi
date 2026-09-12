#include "imu.h"
#include <Wire.h>
#include <math.h>

static uint8_t gImuAddr = MPU6050_ADDR;
static bool gImuReady = false;

static float gAx = 0, gAy = 0, gAz = 1.0f;
static float gRoll = 0, gPitch = 0;
static OrientFace gCurrentOrient = ORIENT_UPRIGHT;
static OrientFace gStableOrient = ORIENT_UPRIGHT;
static uint32_t gOrientCandidateTime = 0;
static bool gOrientChanged = false;

static bool gShakeDetected = false;
static uint32_t gLastShakeTime = 0;
static uint32_t gLastPollTime = 0;

static int gSquishX = 0;
static int gSquishY = 0;

static bool writeReg(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(gImuAddr);
  Wire.write(reg);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}

static bool readBytes(uint8_t reg, uint8_t* buffer, uint8_t length) {
  Wire.beginTransmission(gImuAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom((int)gImuAddr, (int)length);
  if (Wire.available() != length) return false;
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool imuBegin(uint8_t addr) {
  gImuAddr = addr;

  // Verify connection by reading WHO_AM_I (reg 0x75, should return 0x68)
  uint8_t who = 0;
  if (!readBytes(0x75, &who, 1)) {
    gImuReady = false;
    return false;
  }

  // Wake up MPU6050 (clear SLEEP bit in PWR_MGMT_1)
  if (!writeReg(0x6B, 0x00)) {
    gImuReady = false;
    return false;
  }
  delay(10);

  // Set sample rate / config
  writeReg(0x1A, 0x03); // DLPF ~42Hz
  writeReg(0x1C, 0x00); // Accel ±2g

  gImuReady = true;
  return true;
}

void imuUpdate(uint32_t now) {
  if (!gImuReady) return;
  if (now - gLastPollTime < 25) return; // ~40 Hz polling rate
  gLastPollTime = now;

  uint8_t buf[6];
  if (!readBytes(0x3B, buf, 6)) return;

  int16_t rawX = (int16_t)((buf[0] << 8) | buf[1]);
  int16_t rawY = (int16_t)((buf[2] << 8) | buf[3]);
  int16_t rawZ = (int16_t)((buf[4] << 8) | buf[5]);

  // Convert to g (±2g = 16384 LSB/g)
  float ax = (float)rawX / 16384.0f;
  float ay = (float)rawY / 16384.0f;
  float az = (float)rawZ / 16384.0f;

  // Low-pass filter for smooth animation
  gAx = gAx * 0.7f + ax * 0.3f;
  gAy = gAy * 0.7f + ay * 0.3f;
  gAz = gAz * 0.7f + az * 0.3f;

  // Roll and Pitch in degrees
  gRoll  = atan2(gAy, gAz) * 180.0f / M_PI;
  gPitch = atan2(-gAx, sqrt(gAy * gAy + gAz * gAz)) * 180.0f / M_PI;

  // Calculate squish pixel shifts for OLED screen (clamp to reasonable ranges)
  // Ax/Ay roughly -1.0 to 1.0 -> map to squish X (-24 to +24) and Y (-12 to +12)
  float targetSquishX = gAy * 26.0f;
  if (targetSquishX > 24.0f) targetSquishX = 24.0f;
  if (targetSquishX < -24.0f) targetSquishX = -24.0f;
  gSquishX = (int)targetSquishX;

  float targetSquishY = -gAx * 14.0f;
  if (targetSquishY > 12.0f) targetSquishY = 12.0f;
  if (targetSquishY < -12.0f) targetSquishY = -12.0f;
  gSquishY = (int)targetSquishY;

  // Shake detection: total acceleration magnitude
  float mag = sqrt(ax * ax + ay * ay + az * az);
  if (mag > 2.3f || mag < 0.2f) { // Spike or freefall
    if (now - gLastShakeTime > 1200) {
      gShakeDetected = true;
      gLastShakeTime = now;
    }
  }

  // Determine candidate orientation face
  OrientFace candidate = ORIENT_UPRIGHT;
  if (gAz < -0.65f) {
    candidate = ORIENT_UPSIDE_DOWN;
  } else if (gAy > 0.55f) {
    candidate = ORIENT_TILT_RIGHT;
  } else if (gAy < -0.55f) {
    candidate = ORIENT_TILT_LEFT;
  } else if (gAx < -0.55f) {
    candidate = ORIENT_TILT_FRONT;
  } else if (gAx > 0.55f) {
    candidate = ORIENT_TILT_BACK;
  } else {
    candidate = ORIENT_UPRIGHT;
  }

  // Debounce orientation transition (must stay stable for 250ms)
  if (candidate != gCurrentOrient) {
    gCurrentOrient = candidate;
    gOrientCandidateTime = now;
  } else if ((now - gOrientCandidateTime) >= 250 && gCurrentOrient != gStableOrient) {
    gStableOrient = gCurrentOrient;
    gOrientChanged = true;
  }
}

OrientFace imuGetOrientation() {
  return gStableOrient;
}

bool imuTookOrientationChange(OrientFace &newFace) {
  if (gOrientChanged) {
    gOrientChanged = false;
    newFace = gStableOrient;
    return true;
  }
  return false;
}

// Maps 4 horizontal orientations to Pomodoro preset indices (0..3)
int imuGetPomoPresetIndex() {
  switch (gStableOrient) {
    case ORIENT_UPRIGHT:    return 0; // Preset 1 (Classic 25-5)
    case ORIENT_TILT_RIGHT: return 1; // Preset 2 (Deep Work 50-10)
    case ORIENT_TILT_BACK:  return 2; // Preset 3 (Sprint 15-3)
    case ORIENT_TILT_LEFT:  return 3; // Preset 4 (Ultradian 90-20)
    default:                return 0;
  }
}

int imuSquishOffsetX() { return gSquishX; }
int imuSquishOffsetY() { return gSquishY; }
float imuGetRoll()      { return gRoll; }
float imuGetPitch()     { return gPitch; }

bool imuTookShake() {
  bool s = gShakeDetected;
  gShakeDetected = false;
  return s;
}

bool imuIsShaking() {
  return (millis() - gLastShakeTime < 1000);
}

bool imuIsAvailable() {
  return gImuReady;
}
