# StudyMochi User Manual

StudyMochi is a Bengali-speaking desk companion with a clock, weather display,
countdown timer, stopwatch, voice assistant, six-orientation Pomodoro timer,
and an interactive pet.

## 1. Important safety information

- Power StudyMochi from a stable regulated 5 V, 2 A supply.
- The ESP32, MAX98357A, sensors, SD reader, and power supply must share GND.
- Connect the speaker only between the MAX98357A `SPK+` and `SPK-` terminals.
  Never connect either speaker terminal to GND or directly to an ESP32 pin.
- Do not power the device from USB and an external 5 V supply simultaneously
  unless the ESP32 board provides safe power isolation.
- This product uses a passive buzzer. An active buzzer cannot reproduce the
  different notification pitches.

## 2. Controls

StudyMochi has two touch areas.

### Side touch sensor

| Gesture | Action |
|---|---|
| Hold for 2 seconds | Move to the next main mode |
| Short tap in Clock mode | Move to the next clock/weather page |
| Short tap in Timer mode | Switch between countdown and stopwatch |
| Short tap in AI mode | Cancel listening or current local playback |
| Short tap in Pet Pomodoro | Dismiss the current pet reaction |

The four main modes repeat in this order:

1. Clock and weather
2. Timer and stopwatch
3. AI voice assistant
4. Pet Pomodoro

### Head touch sensor

| Current screen | Short tap | Hold for 2 seconds |
|---|---|---|
| Clock/weather | No action | No action |
| Countdown setup | Add 5 minutes | Start countdown |
| Countdown running | No action | Pause or resume |
| Countdown finished | Clear timer | Clear timer |
| Stopwatch | Start or pause | Reset to zero |
| AI assistant | Start or stop listening | Start or stop listening |
| Pet Pomodoro | Happy reaction | Sad reaction |

Three quick head taps within approximately 1.5 seconds produce the angry pet
reaction. Pet gestures work only in Pet Pomodoro mode.

## 3. First-time setup

1. Insert the prepared microSD card before switching on the device.
2. Connect the 5 V power supply.
3. StudyMochi starts immediately in offline mode. The clock, timer, stopwatch,
   Pet Pomodoro, pet reactions, buzzer, OLED, RTC, and SD announcements do not
   wait for Wi-Fi.
4. To configure online features, short-press the GPIO 4 configuration/reset
   button. The OLED displays the setup screen and StudyMochi creates:

   ```text
   Network:  StudyMochi-Direct
   Password: mochi1234
   ```

5. Connect a phone or computer to that network.
6. The setup page should open automatically. If it does not, open
   `http://192.168.4.1` in a browser.
7. Select the normal Wi-Fi network and enter its password.
8. Enter the Gemini API key.
9. Enter latitude and longitude for local weather, or retain the Dhaka values.
10. Save and wait for StudyMochi to connect.

After configuration, the device reconnects automatically whenever it starts.
The RTC clock continues working without Wi-Fi, but weather updates and the AI
assistant require an internet connection.

Short-press GPIO 4 again whenever you want to change Wi-Fi, the API key, or
the weather location. Closing or timing out the portal without connecting does
not stop offline operation.

## 4. Sound behavior

StudyMochi has two independent sound devices:

- The passive buzzer provides button feedback, timer cues, pet reactions, and
  other short notifications.
- The MAX98357A and speaker play Bangla announcements and Gemini speech.

When a mode or Pomodoro announcement requires both, StudyMochi plays the
buzzer first, waits briefly, and then plays the Bangla speech. This prevents
the two sounds from covering each other.

## 5. Mode 1: Clock and weather

Hold the side sensor until Clock mode is selected. Short-tap the side sensor to
move through four pages:

1. Time, date, weekday, and time-of-day label
2. Current temperature and weather condition
3. Feels-like temperature, humidity, and wind speed
4. Today's maximum, minimum, and rain probability

Small dots show which page is selected. Weather normally refreshes every 15
minutes when Wi-Fi is available.

## 6. Mode 2: Timer and stopwatch

### Countdown timer

1. Select Timer mode and make sure the countdown page is visible.
2. Tap the head sensor once to select 5 minutes.
3. Continue tapping to select 10, 15, 20, and so on up to 60 minutes.
4. Holding the head sensor starts the selected countdown.
5. Hold again while running to pause. Hold once more to resume.
6. When time expires, the display flashes and the buzzer plays the completion
   pattern.
7. Tap or hold the head sensor to clear the completed timer.

After 60 minutes, another setup tap returns the selection to zero and cancels
the setup.

### Stopwatch

1. Short-tap the side sensor to open the stopwatch page.
2. Short-tap the head sensor to start.
3. Short-tap again to pause or resume.
4. Hold the head sensor for two seconds to reset to zero.

The countdown and stopwatch continue updating when another page is displayed.

## 7. Mode 3: AI voice assistant

The AI assistant requires configured Wi-Fi, internet access, and a valid Gemini
API key.

1. Select AI mode.
2. Tap or hold the head sensor to enable listening.
3. Wait for the buzzer's start cue to finish, then speak naturally.
4. Tap or hold the head sensor again when finished.
5. StudyMochi displays a thinking state and answers through the speaker.

The on-board BOOT button is also available as a hold-to-talk backup. A low
error pattern means the online AI session is not ready.

## 8. Mode 4: Pet Pomodoro

Each physical orientation selects a different automatic Pomodoro profile.

| Physical placement | MPU6050 axis | Work/break profile |
|---|---|---|
| Normal upright | Z- | Classic 25/5 |
| Physical right side | Y- | Deep Work 50/10 |
| Physical back side | X+ | Sprint 15/3 |
| Physical left side | Y+ | Extended 90/20 |
| Physical front side | X- | Balanced 30/5 |
| Upside down | Z+ | Focus 60/10 |

To start a session:

1. Select Pet Pomodoro mode.
2. Place StudyMochi steadily in the orientation for the required profile.
3. The display begins a 10-second preparation countdown.
4. After 10 seconds, the work timer starts automatically.

Changing to another stable orientation stops and resets the current session,
selects the new profile, and begins another 10-second countdown. Leaving Pet
Pomodoro mode also stops and resets the session.

The timer display automatically rotates after the new orientation has been
stable for approximately 700 ms. Right and left placements use a portrait
layout; opposite faces use a 180-degree layout where required. This keeps the
timer and text horizontal for each of the six calibrated placements.

During the session:

- One head tap shows the happy face and plays the happy buzzer pattern.
- Three quick head taps show the angry face and play the angry pattern.
- A two-second head hold shows the sad face and plays the sad pattern.
- The timer continues while a pet face is displayed.
- A side tap dismisses the pet face and returns to the timer.

Work, break, and session transitions play a buzzer cue first and then the
corresponding Bangla announcement from the speaker.

## 9. Automatic do-not-disturb

Outside Pet Pomodoro mode, placing StudyMochi upside down enables
do-not-disturb:

- The OLED switches off.
- Local audio and microphone input stop.
- Buzzer feedback is muted.
- Active timers continue in the background.

Return the device to another orientation to restore the display and sound.
In Pet Pomodoro mode, upside down is the sixth Pomodoro profile and does not
activate do-not-disturb.

## 10. microSD card requirements

Use a FAT32-formatted microSD card. The card must contain this directory and
these files:

```text
/audio/0003_pomo_start.wav
/audio/0004_break_start.wav
/audio/0005_session_done.wav
/audio/0006_clock_mode.wav
/audio/0007_timer_mode.wav
/audio/0008_ai_mode.wav
```

The WAV files must be 24 kHz, 16-bit, mono PCM. Buzzer and pet sounds do not
require files on the SD card.

## 11. Factory reset

The GPIO 4 button has two actions:

- Short press: open the setup portal and keep existing settings.
- Hold for three seconds: clear Wi-Fi, API key, and device configuration; open
  the setup portal; then restart after setup closes.

Factory reset cannot be undone. Do not use it for an ordinary restart.

## 12. Troubleshooting

### No passive-buzzer sound

- Confirm that the buzzer is passive, not active.
- Confirm signal on GPIO 5 and a common GND.
- For a small piezo, use a 100–220 Ω series resistor.
- Use a transistor driver for a magnetic buzzer drawing more than about 10 mA.

### No Bangla speech or AI audio

- Check MAX98357A wiring: BCLK 26, LRC 27, DIN 14, VIN 5 V, and common GND.
- Connect the speaker only to `SPK+` and `SPK-`.
- Check that the SD card is inserted and contains the required WAV files.
- Verify Wi-Fi and the API key if Gemini speech is also unavailable.

### Clock works but weather or AI does not

The RTC works offline. Check Wi-Fi, internet access, location settings, and the
Gemini API key.

### Touch does not respond correctly

- Keep touch-sensor wires short and away from speaker and amplifier wires.
- Confirm side sensor on GPIO 18 and head sensor on GPIO 19.
- A hold must remain continuous for approximately two seconds.

### Wrong Pomodoro profile is selected

- Place the product on a flat surface and keep it still for at least one
  second.
- Confirm the MPU6050 is fixed securely and has not rotated inside the case.
- Compare the physical side with the orientation table above.

### OLED is unexpectedly off

Outside Pet Pomodoro mode, upside-down placement intentionally activates
do-not-disturb. Turn the device to another side.

## 13. Normal shutdown

StudyMochi has no software shutdown procedure. Stop any active session if
desired, then disconnect the 5 V supply. The RTC backup battery preserves the
clock while main power is disconnected.
