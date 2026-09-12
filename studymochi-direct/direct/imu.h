// ════════════════════════════════════════════════════════════════
//   MPU6050 6-Axis IMU Module for StudyMochi
//
//   I2C Address: 0x69 (AD0 pulled HIGH to avoid DS3231 0x68 conflict)
//   Shares SDA (GPIO 21) & SCL (GPIO 22) with OLED and DS3231.
//
//   Features:
//     - Orientation detection (Upright, Left, Right, Back, Front, Upside-Down)
//     - 4-Side selection mapping for Pomodoro presets
//     - Dynamic tilt angle & pixel squish offsets for face squish physics
//     - Violent shake detection for dizzy pet animation
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>

#define MPU6050_ADDR 0x69  // AD0 pulled HIGH

enum OrientFace {
  ORIENT_UPRIGHT     = 0,  // Sitting normal on desk (Preset 1: Classic 25-5)
  ORIENT_TILT_RIGHT  = 1,  // Tilted/placed right   (Preset 2: Deep Work 50-10)
  ORIENT_TILT_BACK   = 2,  // Tilted/placed back    (Preset 3: Quick Sprint 15-3)
  ORIENT_TILT_LEFT   = 3,  // Tilted/placed left    (Preset 4: Ultradian 90-20)
  ORIENT_TILT_FRONT  = 4,  // Tilted forward
  ORIENT_UPSIDE_DOWN = 5   // Flipped upside down (Sleep mode)
};

bool imuBegin(uint8_t addr = MPU6050_ADDR);
void imuUpdate(uint32_t now);

// Orientation queries
OrientFace imuGetOrientation();
bool imuTookOrientationChange(OrientFace &newFace);
int  imuGetPomoPresetIndex(); // Maps orientation directly to preset 0..3

// Squish physics queries (returns pixel shift for OLED face deformation)
int  imuSquishOffsetX(); // Negative = squish left, Positive = squish right (-24 to +24 px)
int  imuSquishOffsetY(); // Negative = squish up, Positive = squish down (-12 to +12 px)
float imuGetRoll();
float imuGetPitch();

// Shake detection
bool imuTookShake();
bool imuIsShaking();
bool imuIsAvailable();
