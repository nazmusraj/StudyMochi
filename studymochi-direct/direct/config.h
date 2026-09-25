#pragma once

// Central hardware and service configuration for the ESP32 DevKit (WROOM).
// Keep pin assignments here so wiring and firmware cannot silently diverge.

#define MOCHI_AP_NAME "StudyMochi-Direct"
#define MOCHI_AP_PASS "mochi1234"

#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define OLED_I2C_ADDR 0x3C

#define PIN_TOUCH_SIDE 18
#define PIN_TOUCH_HEAD 19

#define PIN_MIC_BCLK 33
#define PIN_MIC_LRCLK 25
#define PIN_MIC_DATA 32

#define PIN_AMP_BCLK 26
#define PIN_AMP_LRCLK 27
#define PIN_AMP_DATA 14

// Custom SPI bus. GPIO 16/17 become available after removing DFPlayer.
#define PIN_SD_SCK 23
#define PIN_SD_MOSI 17
#define PIN_SD_MISO 16
#define PIN_SD_CS 13

#define PIN_FACTORY_RESET 4
#define PIN_BOOT_BUTTON 0
#define PIN_STATUS_LED 2
#define PIN_PASSIVE_BUZZER 5

#define MIC_SAMPLE_RATE 16000
#define SPEAKER_SAMPLE_RATE 24000

#define TOUCH_HOLD_MS 2000UL
#define PET_RAPID_TAP_COUNT 3
#define PET_RAPID_TAP_WINDOW_MS 1500UL

#define WEATHER_REFRESH_MS (15UL * 60UL * 1000UL)
#define NTP_REFRESH_MS (10UL * 60UL * 1000UL)

#define DEFAULT_LATITUDE 23.8103f
#define DEFAULT_LONGITUDE 90.4125f

#define AUDIO_POMO_START "/audio/0003_pomo_start.wav"
#define AUDIO_BREAK_START "/audio/0004_break_start.wav"
#define AUDIO_SESSION_DONE "/audio/0005_session_done.wav"
#define AUDIO_CLOCK_MODE "/audio/0006_clock_mode.wav"
#define AUDIO_TIMER_MODE "/audio/0007_timer_mode.wav"
#define AUDIO_AI_MODE "/audio/0008_ai_mode.wav"
