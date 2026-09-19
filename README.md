# StudyMochi

StudyMochi is a Bengali-language desk companion built around an ESP32 and a
128×64 monochrome OLED. It combines a clock, three-page weather view,
Pomodoro timer, normal countdown timer, stopwatch, pet reactions, six-face
MPU6050 interactions, and a Gemini Live voice assistant.

All device-facing text and recorded speech are Bangla. Source comments,
diagnostics, tooling, and documentation are English.

## Hardware

- ESP32 DevKit based on the classic ESP32-WROOM module
- 128×64 SH1106 or SSD1306 I²C OLED
- Two TTP223 capacitive touch modules
- DS3231 RTC with backup battery
- MPU6050 IMU configured at address `0x69`
- INMP441 I²S microphone
- MAX98357A I²S amplifier and one 4-ohm, 3-watt speaker
- 3.3 V logic-compatible microSD reader
- Regulated 5 V, 2 A power supply

## Final wiring

### Shared I²C bus

| ESP32 | OLED | DS3231 | MPU6050 |
|---|---|---|---|
| GPIO 21 | SDA | SDA | SDA |
| GPIO 22 | SCL | SCL | SCL |
| 3V3 | VCC | VCC | VCC |
| GND | GND | GND | GND |

- OLED address: `0x3C`
- DS3231 address: `0x68`
- Connect MPU6050 `AD0` to `3V3` for address `0x69`.
- Do not allow an I²C breakout to pull SDA or SCL up to 5 V.

### Touch sensors

| Function | TTP223 OUT | Power |
|---|---:|---|
| Side/navigation sensor | GPIO 18 | 3V3 and GND |
| Head/pet sensor | GPIO 19 | 3V3 and GND |

Both sensors are expected to be active high. Keep their wires short and away
from the speaker and amplifier wiring.

### INMP441 microphone

| INMP441 | ESP32 |
|---|---:|
| SCK/BCLK | GPIO 33 |
| WS/LRCL | GPIO 25 |
| SD | GPIO 32 |
| L/R | GND |
| VDD | 3V3 |
| GND | GND |

### MAX98357A amplifier

| MAX98357A | Connection |
|---|---|
| BCLK | GPIO 26 |
| LRC/WS | GPIO 27 |
| DIN | GPIO 14 |
| VIN | 5 V |
| GND | Common GND |
| SPK+ and SPK- | Speaker terminals |

The amplifier output is bridge-tied. Neither speaker terminal may be
connected to ground.

### microSD reader

| microSD signal | ESP32 |
|---|---:|
| SCK/CLK | GPIO 23 |
| MOSI/DI/CMD | GPIO 17 |
| MISO/DO/DAT0 | GPIO 16 |
| CS | GPIO 13 |
| VCC | 3V3 |
| GND | GND |

Use a true 3.3 V logic module. A breakout explicitly designed for 5 V input
may be powered as its manufacturer specifies, but its signal outputs must
remain safe for the ESP32.

### Other pins

| Function | Connection |
|---|---|
| Factory reset button | GPIO 4 to GND |
| On-board BOOT input | GPIO 0 |
| On-board status LED | GPIO 2 |

Holding the GPIO 4 button for three seconds clears Wi-Fi and saved device
configuration. The BOOT button remains a hold-to-talk backup in AI mode.

## Power recommendations

Feed the ESP32 `VIN/5V` pin and MAX98357A from a regulated 5 V, 2 A supply.
Use the ESP32 3.3 V rail for the digital sensors and a compatible microSD
reader. All grounds must be common. A 470–1000 µF capacitor near the
amplifier and a 100 µF plus 0.1 µF pair near the SD reader help with current
spikes and audio noise.

Avoid powering the board from USB and an external 5 V source simultaneously
unless the particular DevKit provides safe power isolation.

## User interface

### Side sensor

- Hold for two seconds: move to the next main mode.
- Short touch in Clock or Timer mode: move to the next subpage.
- Short touch in AI mode: cancel listening or local playback.

### Head sensor

| Context | Short touch | Two-second hold |
|---|---|---|
| Clock/weather | Happy pet reaction | Cuddle reaction |
| Pomodoro, stopped | Select next preset | Start |
| Pomodoro, running/paused | Pet reaction | Pause or resume |
| Normal timer, setting | Add five minutes | Start |
| Normal timer, running/paused | Pet reaction | Pause or resume |
| Stopwatch | Start or pause | Reset |
| AI | Toggle microphone input | Toggle microphone input |

Three fast head taps trigger an angry reaction only in Clock mode. Rapid taps
used for timer setup never trigger anger.

Pet reactions temporarily replace the content screen. Timers continue in the
background, and the exact previous page returns after the animation.

### Modes and pages

1. Clock
   - Clock, date, day, and time-of-day label
   - Current weather: temperature and condition
   - Weather details: feels-like temperature, humidity, and wind
   - Today's weather: maximum, minimum, and rain probability
2. Timer
   - Pomodoro presets: 25/5, 50/10, 15/3, and 90/20
   - Normal countdown in five-minute steps
   - Stopwatch
3. AI
   - Gemini Live Bangla conversation
   - Touch-to-start and touch-to-stop microphone streaming

Weather pages use three tiny dots as the page indicator. Each 128×64 screen
contains one readable information group so Bengali glyphs do not overlap.

## MPU6050 behavior

- A new orientation must remain stable for 700 ms before it is accepted.
- Four supported side orientations select Pomodoro presets while Pomodoro is
  stopped. Running sessions cannot be changed by rotation.
- Face-down placement enables do-not-disturb: microphone input and audio stop,
  and the OLED turns off while timers continue.
- Returning from face-down restores the previous page.
- Side placement produces a soft squish sound.
- A strong shake produces a temporary dizzy reaction with a cooldown.

## Time and weather

The DS3231 is read immediately at boot, so the clock works without Wi-Fi.
When Wi-Fi is available, background SNTP synchronization runs every ten
minutes and writes the corrected local time back to the DS3231.

Open-Meteo is refreshed every 15 minutes and needs no API key. Latitude and
longitude are configured through the Wi-Fi setup portal. The request uses:

- Current: temperature, relative humidity, apparent temperature, day/night,
  WMO weather code, and 10 m wind speed
- Daily: maximum temperature, minimum temperature, and maximum precipitation
  probability
- `timezone=auto`, Celsius, km/h, millimetres, one forecast day

The location's UTC offset returned by Open-Meteo is applied to SNTP. Cached
weather remains visible during a network outage.

## Audio and SD card

DFPlayer is not used. Recorded announcements, generated pet tones, and Gemini
audio all share the MAX98357A and the same speaker through one priority-aware
audio manager.

Format the microSD card as FAT32 and copy the repository's `sdcard/audio`
folder to the card root. The final card must contain:

```text
/audio/0003_pomo_start.wav
/audio/0004_break_start.wav
/audio/0005_session_done.wav
/audio/0006_clock_mode.wav
/audio/0007_timer_mode.wav
/audio/0008_ai_mode.wav
```

All six files are 24 kHz, 16-bit, mono PCM WAV. Pet reactions are synthesized
on the ESP32 and therefore require no additional files.

Playback priority is timer alarm, Gemini output, recorded announcement, then
pet feedback. A higher-priority event can interrupt a lower-priority event.

## Generate the voice files again

The generator reads the credential from an environment variable and never
writes or prints it. Do not put an API key in this repository.

PowerShell:

```powershell
$env:GEMINI_API_KEY = "your-key"
python studymochi-direct/tools/generate_voice_assets.py --output-dir sdcard/audio --overwrite
Remove-Item Env:\GEMINI_API_KEY
```

The script uses Gemini 3.1 Flash TTS, the youthful `Leda` voice, Bangladeshi
Bangla direction, free-tier rate-limit backoff, and WAV format validation.

## Build and upload

Required software:

- Arduino IDE 2 or Arduino CLI
- ESP32 board package 3.3.11 or later
- WiFiManager by tzapu

Open `studymochi-direct/direct/direct.ino`, select a classic ESP32 Dev Module,
and select the **Huge APP (3 MB No OTA / 1 MB SPIFFS)** partition scheme when
available. The current firmware also fits the default 1.25 MB application
partition, but with little spare flash space.

Arduino CLI example:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 studymochi-direct/direct
arduino-cli upload --fqbn esp32:esp32:esp32 -p COM_PORT studymochi-direct/direct
```

On first boot, connect a phone to:

```text
SSID: StudyMochi-Direct
Password: mochi1234
```

Use the captive portal to select Wi-Fi, enter the device's Gemini Live API key,
and set latitude and longitude. Configuration is stored in ESP32 NVS. No
Open-Meteo key is required.

## Bengali bitmap generation

The OLED driver uses pre-rendered Noto Sans Bengali Bold bitmaps because simple
embedded fonts do not shape Bengali conjuncts reliably. The generator uses
Pillow with libraqm or HarfBuzz plus FreeType and rejects normal labels wider
than 124 pixels or large labels wider than 118 pixels.

```powershell
python studymochi-direct/gen_bangla.py
```

The font and SIL Open Font License are stored in `studymochi-direct/assets`.

## Serial commands

- `i`: print status
- `s`: play speaker test sound
- `c`: request background time synchronization
- `o`: switch SH1106/SSD1306 addressing
- `v 0..100`: set speaker volume
- `g value`: set microphone gain
- `t question`: test Gemini using text
- `k`: clear the normal timer
- `p`: reopen the configuration portal
- `r`: factory reset

## Verification status

The complete sketch compiles for `esp32:esp32:esp32` with Arduino-ESP32 3.3.11.
Hardware verification should cover SD startup, every WAV file, both touch
sensors, all six stable orientations, RTC recovery after power removal,
10-minute SNTP correction, weather caching, timer alarms, and AI interruption.
