# 🌸 StudyMochi v2 — AI Study Companion & Desktop Pet

**StudyMochi v2** is a standalone, laptop-free desktop AI study companion powered by ESP32, Gemini Live API, and custom tactile & inertial interaction engines. It features a kawaii animated pet face on an OLED screen, rich Bangla voice feedback via DFPlayer Mini, gravity-based face-squish physics with MPU6050, 4-orientation Pomodoro switching, a millisecond stopwatch, and an intuitive dual-touch navigation system.

---

## 📑 Table of Contents
1. [Key Features](#-key-features)
2. [Pinout & Hardware Wiring](#-pinout--hardware-wiring)
3. [DFPlayer Mini & Bangla Voice Files](#-dfplayer-mini--bangla-voice-files)
4. [Interaction & Navigation Guide](#-interaction--navigation-guide)
5. [MPU6050 Orientation & Physics](#-mpu6050-orientation--physics)
6. [Extensible Pomodoro Engine](#-extensible-pomodoro-engine)
7. [First Time Setup & Wi-Fi Configuration](#-first-time-setup--wi-fi-configuration)
8. [Arduino IDE Flashing Settings](#-arduino-ide-flashing-settings)
9. [Serial Monitor Commands](#-serial-monitor-commands)
10. [Codebase Architecture](#-codebase-architecture)

---

## ✨ Key Features

- **Gemini Live AI Assistant**: Bidirectional low-latency streaming chat with mouth lip-sync animation (no computer or external server required).
- **Bangla Voice Engine**: High-fidelity Bangla voice alerts, pet cuddles, and quirky protests via DFPlayer Mini.
- **Cute Pet Personality & Mood Engine**:
  - Single pat ➔ Happy smile + sweet chirp.
  - 2.0s hold ➔ Cuddle reaction with floating heart eyes (`♥_♥`), purring, and loving voice.
  - 3 rapid taps (≤1.5s) ➔ Mochi gets annoyed! Angry cross eyes (`X_X`), jagged mouth, and protest vocalization.
- **MPU6050 Motion Dynamics**:
  - **4-Orientation Pomodoro**: Turn the device on any of its 4 horizontal sides to immediately switch Pomodoro presets!
  - **Side-Squish Gravity Physics**: In Clock/Pet mode, tilting the device left or right compresses Mochi's facial geometry against the OLED border with comical complaints (*"আরে পড়ে যাচ্ছি! চ্যাপ্টা হয়ে গেলাম!"*).
  - **Shaken / Dizzy**: Shaking triggers dizzy spinning eyes (`@_@`).
- **Mode & Sub-Mode Architecture**:
  - **Mode 1: Clock & Pet Mode** (Sub 1: Real-time clock/date from DS3231 + NTP, Sub 2: Open-Meteo live weather).
  - **Mode 2: Timers & Study Mode** (Sub 1: Modular Pomodoro, Sub 2: 5-min adjustable countdown, Sub 3: Millisecond Stopwatch).
  - **Mode 3: AI Assistant Mode** (Direct Gemini Live voice mode).
- **Extensible Pomodoro Engine**: Modular profile system supporting custom work/break/round configurations.

---

## 🔌 Pinout & Hardware Wiring

All peripherals connect directly to the ESP32:

| Peripheral | Pins | ESP32 GPIO | Notes |
|---|---|---|---|
| **INMP441 I2S Mic** | SCK / WS / SD | **SCK=33, WS=25, SD=32** | VDD ➔ 3.3V, GND ➔ GND, L/R ➔ GND |
| **MAX98357A I2S Amp** | BCLK / LRC / DIN | **BCLK=26, LRC=27, DIN=14** | VIN ➔ 5V, GAIN ➔ GND, SD ➔ VIN |
| **OLED (SH1106 1.3")** | SDA / SCL | **SDA=21, SCL=22** | I2C Address `0x3C` |
| **DS3231 RTC Module** | SDA / SCL | **SDA=21, SCL=22** | I2C Address `0x68` (battery backed) |
| **MPU6050 6-Axis IMU** | SDA / SCL / **AD0** | **SDA=21, SCL=22, AD0 ➔ 3.3V** | **Address `0x69`** (AD0 HIGH avoids conflict with RTC) |
| **DFPlayer Mini Voice** | TX / RX | **RX2=16, TX2=17** | ESP32 GPIO 16 ◄── DFPlayer TX<br/>ESP32 GPIO 17 ──► 1kΩ ──► DFPlayer RX<br/>VCC ➔ 5V, GND ➔ GND |
| **TTP223 Top Touch** | SIG (OUT) | **GPIO 18** | Head touch: Cuddle / Angry tap / Context action |
| **TTP223 Side Touch** | SIG (OUT) | **GPIO 19** | Menu touch: 2s hold = Main Mode, Tap = Sub-Mode |
| **BOOT Button** | On-board | **GPIO 0** | Universal talk backup |
| **Status LED** | On-board | **GPIO 2** | WiFi / activity indicator |
| **Factory Reset Button** | External/Push | **GPIO 4** | Hold 3 seconds to clear WiFi & API key |

> [!IMPORTANT]
> **MPU6050 AD0 Pin:** Make sure the **AD0 pin** on the MPU6050 breakout is connected to **3.3V (VCC)**. This configures the MPU6050 to I2C address `0x69`, preventing any address collision with the DS3231 RTC (`0x68`).

---

## 🔊 DFPlayer Mini & Bangla Voice Files

Format your microSD card as **FAT32**. Create a folder named `mp3` in the root directory and place the following numbered audio files:

```
/mp3/
  ├── 0001.mp3  -> Happy cuddle ("খুব ভালো লাগছে! ভালোবাসি তোমাকে!")
  ├── 0002.mp3  -> Angry tap    ("উফ! মাথায় এভাবে মারছ কেন? রাগ হচ্ছে!")
  ├── 0003.mp3  -> Pomo start   ("পড়ার সময় শুরু! বইয়ে মন দাও।")
  ├── 0004.mp3  -> Pomo break   ("ব্রেক টাইম! একটু পানি খেয়ে নাও আর চোখ বিশ্রাম দাও।")
  ├── 0005.mp3  -> Pomo finish  ("সাবাশ! আজকের স্টাডি সেশন দারুণভাবে শেষ হলো!")
  ├── 0006.mp3  -> Mode clock   ("ঘড়ি মোড")
  ├── 0007.mp3  -> Mode timer   ("টাইমার ও পোমোডোরো মোড")
  ├── 0008.mp3  -> Mode AI      ("এআই মোড চালু হয়েছে")
  ├── 0009.mp3  -> Squish left  ("আরে আরে! বামে কাত হয়ে গেলাম!")
  ├── 0010.mp3  -> Squish right ("ডানে কাত হয়ে চ্যাপ্টা হয়ে গেলাম, সোজা করো!")
  └── 0011.mp3  -> Shaken dizzy ("মাথা ঘুরছে! থামাও থামাও!")
```

---

## 🎮 Interaction & Navigation Guide

StudyMochi uses two capacitive touch sensors:

```
                  ┌────────────────────────┐
                  │   HEAD TOUCH (GPIO 19) │  <-- Other Touch: Pet, Cuddle, Voice Input in AI Mode
                  └────────────────────────┘
                  ┌────────────────────────┐
                  │                        │
                  │       OLED SCREEN      │  [SIDE TOUCH] (GPIO 18)
                  │       (128 x 64)       │  <-- Normal Touch: 2s hold = Main Mode
                  │                        │      Short tap = Sub-Mode (Exits AI mode)
                  └────────────────────────┘
```

> [!TIP]
> **Reversed pins?** If your breadboard or enclosure has the sensors wired to the opposite pins, you don't need to resolder! Simply open the Serial Monitor (115200 baud) and type `w` followed by `ENTER`. StudyMochi will swap the pin assignments (`18 <-> 19`) and persist your preference in ESP32 NVS flash memory.

### 1. Side Touch Sensor (GPIO 18 — Normal Touch / Mode Navigation)
- **Long Press (≥ 2.0s):** Cycles the **Main Modes**:
  1. `MODE_CLOCK` (Clock / Weather / Pet)
  2. `MODE_TIMER` (Pomodoro / Custom Timer / Stopwatch)
  3. `MODE_AI` (Gemini Live Mode)
- **Short Tap:** Cycles **Sub-Modes** within the active mode:
  - In `MODE_CLOCK`: Sub 1 (Clock, Date, Day) ↔ Sub 2 (Live Weather)
  - In `MODE_TIMER`: Sub 1 (Pomodoro) ➔ Sub 2 (Custom Timer) ➔ Sub 3 (Stopwatch)
  - In `MODE_AI`: **Immediately exits AI mode back to Clock mode!**
- **Interruption Guard:** If touched while Gemini voice recording or playback is active, it immediately aborts speech and smoothly advances the mode.
- **Strict Isolation:** This sensor **never** triggers voice input under any circumstance.

### 2. Top / Head Touch Sensor (GPIO 19 — Other Touch / Pet & Voice)
- **In AI Mode (`MODE_AI`):**
  - **Press and Hold:** Speaks to Gemini Live API over WebSocket (LED turns ON, listening animation).
  - **Release:** Automatically sends turn to Gemini Live; plays the Bengali/English spoken response through the speaker with mouth lip-sync animation.
- **In Clock Mode (Pet Interaction):**
  - **Single Soft Tap:** Happy pet smile (`+10` mood).
  - **Hold (≥ 2.0s):** Cuddle response! Heart eyes (`♥_♥`) and Bangla loving voice (`+25` mood).
  - **3 Rapid Taps (≤ 1.5s):** Mochi gets angry! Cross eyes (`X_X`), jagged mouth, and protest voice (`-35` mood).
- **In Timer Mode:**
  - **Pomodoro Screen:**
    - Tap: Start / Pause current session.
    - Hold (2s): Advance to next preset profile manually.
  - **Custom Timer Screen:**
    - Tap: Adjust duration (+5 min increments).
    - Hold (2s): Start / pause countdown.
  - **Stopwatch Screen:**
    - Tap: Start / Pause stopwatch.
    - Hold (2s): Reset stopwatch to `00:00.00`.

---

## 🔄 MPU6050 Orientation & Physics

### 1. Four-Orientation Pomodoro Switching
When in Pomodoro mode, resting or turning the device on any horizontal side selects between 4 popular presets automatically:

| Orientation Face | Preset Profile | Work / Break | Recommended Use |
|---|---|---|---|
| **Upright / Front** | **25-5 Classic** | 25 min work, 5 min break | Standard Pomodoro technique |
| **Right Side** | **50-10 Deep Work** | 50 min work, 10 min break | Deep concentration & thesis writing |
| **Back Side** | **15-3 Sprint** | 15 min work, 3 min break | Quick flashcards & rapid revision |
| **Left Side** | **90-20 Extended** | 90 min work, 20 min break | Ultradian rhythm & mock exams |

### 2. Side-Squish Face Physics
In Clock / Pet Mode, tilting the device left or right applies real-time gravity physics to Mochi's facial features:
- Eyes and cheeks squash against that edge of the OLED screen.
- Triggers comical Bangla vocal complaints (*Track 0009 / 0010*).
- Restoring upright springs the face back to center.

### 3. Shake Detection
Shaking the device vigorously causes Mochi to get dizzy:
- Eyes become spinning spirals (`@_@`).
- Plays dizzy Bangla audio (*Track 0011*).

---

## ⏱ Extensible Pomodoro Engine

To add or modify Pomodoro profiles, edit `POMO_PROFILES` in [`studymochi-direct/direct/pomodoro_engine.h`](file:///d:/L4-T1/EEE-416%20Project/StudyMochi/studymochi-direct/direct/pomodoro_engine.h):

```cpp
static const PomoProfile POMO_PROFILES[] = {
  { "CLASSIC",   "25-5",  "Classic",   25 * 60,  5 * 60, 15 * 60, 4 },
  { "DEEP",      "50-10", "Deep Work", 50 * 60, 10 * 60, 20 * 60, 4 },
  { "SPRINT",    "15-3",  "Sprint",    15 * 60,  3 * 60, 10 * 60, 4 },
  { "ULTRADIAN", "90-20", "Extended",  90 * 60, 20 * 60, 30 * 60, 3 },
  // Add your own custom profile here:
  // { "CUSTOM", "40-8", "My Focus", 40 * 60, 8 * 60, 15 * 60, 3 }
};
```

---

## 📱 First Time Setup & Wi-Fi Configuration

1. Power on StudyMochi. If no Wi-Fi credentials or API key are stored, the hotspot portal starts automatically.
2. The OLED will display: **"সেটআপ / ফোন দিয়ে জুড়ুন"**.
3. Connect your phone or laptop to the Wi-Fi network:
   - **SSID:** `StudyMochi-Direct`
   - **Password:** `mochi1234`
4. The captive portal page will open automatically (or browse to `192.168.4.1`).
5. Select your home Wi-Fi network, enter its password, and paste your **Gemini API Key**.
6. (Optional) Set your custom latitude/longitude for weather (default is Dhaka: `23.8103, 90.4125`).
7. Click **Save**. StudyMochi reboots, connects to Wi-Fi, syncs time from NTP to the DS3231 RTC, and enters Clock mode!

---

## 🛠 Arduino IDE Flashing Settings

### Libraries to Install:
Open **Arduino IDE ➔ Tools ➔ Manage Libraries**:
1. **WiFiManager** (by tzapu)
2. **RTClib** (by Adafruit) — choose *"Install all dependencies"* to include Adafruit BusIO.

*(Note: Custom zero-copy OLED drivers and custom WebSocket clients are built into the sketch. No external display or WebSocket libraries are required).*

### Board Configuration:
- **Board**: `ESP32 Dev Module`
- **Upload Speed**: `921600` (or `115200` if unstable)
- **Flash Size**: `4MB (32Mb)`
- **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)` or `Default 4MB with spiffs`
- **PSRAM**: `Disabled`
- **Port**: Select the COM port corresponding to your ESP32.

---

## 🖥 Serial Monitor Commands (115200 Baud)

Type any of these commands in the Serial Monitor and press **ENTER**:

| Command | Action |
|---|---|
| `s` | Test speaker chime (plays test tone) |
| `v <0-100>` | Adjust speaker volume (e.g. `v 65`) |
| `t <question>` | Ask Gemini via text prompt without microphone (e.g. `t Newton er sutro ki`) |
| `g <gain>` | Adjust microphone gain manually (e.g. `g 6`) |
| `o` | Toggle OLED controller between SH1106 (1.3") and SSD1306 (0.96") |
| `i` | Print full system diagnostic info (heap, battery, WiFi, audio levels) |
| `p` | Open WiFi & API key configuration portal |
| `r` | Factory reset (clears WiFi credentials and API key) |
| `k` | Clear and reset current timer |

---

## 📂 Codebase Architecture

```
studymochi-direct/direct/
  ├── direct.ino          # Main sketch: setup, loop, I2S audio & Gemini Live client
  ├── modes.h / .cpp      # Central Mode & Sub-Mode navigation state machine
  ├── imu.h / .cpp        # MPU6050 driver (0x69): 4-orientation detection, tilt squish, shake
  ├── dfvoice.h / .cpp    # DFPlayer Mini Bangla voice player over HardwareSerial2
  ├── pomodoro_engine.h/.cpp # Extensible modular Pomodoro system & phase management
  ├── stopwatch.h / .cpp  # Centisecond-accurate stopwatch engine
  ├── mood.h / .cpp       # Pet mood engine: score (-100 to +100), cuddle, anger, decay
  ├── touch.h / .cpp      # TTP223 debouncer with 2.0s hold and 3-tap rapid-tap detection
  ├── face.h / .cpp       # Fast windowed OLED driver, kawaii expressions, squish physics
  ├── rtcclock.h / .cpp   # DS3231 RTC driver with NTP synchronization and Bengali numbers
  ├── weather.h / .cpp    # Open-Meteo REST client with 15-minute smart caching
  ├── banglabmp.h         # Pre-rendered Bengali text bitmaps
  └── miniws.h / .cpp     # Low-memory streaming WebSocket client (RFC 6455)
```
