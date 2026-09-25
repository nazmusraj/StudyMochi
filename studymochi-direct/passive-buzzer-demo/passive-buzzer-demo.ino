#include <Arduino.h>

// StudyMochi passive-buzzer test.
// This sketch does not use the MAX98357A, speaker, SD card, or I2S.
constexpr uint8_t BUZZER_PIN = 5;

struct Note {
  uint16_t frequency;
  uint16_t durationMs;
  uint16_t gapMs;
};

const Note LONG_TONE[] = {{1000, 2000, 0}};

const Note MODE_CLOCK[] = {{900, 350, 120}, {1200, 500, 0}};
const Note MODE_TIMER[] = {{1250, 350, 120}, {1250, 500, 0}};
const Note MODE_AI[] = {{1200, 350, 120}, {1750, 500, 0}};
const Note MODE_POMO[] = {
    {1000, 300, 100}, {1400, 300, 100}, {1900, 500, 0}};

const Note PET_HAPPY[] = {{740, 350, 120}, {990, 600, 0}};
const Note PET_ANGRY[] = {
    {420, 350, 100}, {330, 450, 100}, {420, 600, 0}};
const Note PET_SAD[] = {{900, 500, 150}, {650, 800, 0}};

const Note START_CUE[] = {{700, 350, 120}, {1100, 550, 0}};
const Note PAUSE_CUE[] = {{1100, 350, 120}, {700, 550, 0}};
const Note DONE_CUE[] = {
    {880, 400, 150}, {1175, 450, 150}, {1568, 800, 0}};
const Note ERROR_CUE[] = {{360, 450, 180}, {300, 650, 0}};

void playPattern(const char *name, const Note *notes, size_t noteCount) {
  Serial.printf("Playing: %s\n", name);
  for (size_t i = 0; i < noteCount; ++i) {
    tone(BUZZER_PIN, notes[i].frequency, notes[i].durationMs);
    delay(notes[i].durationMs);
    noTone(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, LOW);
    if (notes[i].gapMs)
      delay(notes[i].gapMs);
  }
  delay(800);
}

#define PLAY_PATTERN(name, notes)                                             \
  playPattern(name, notes, sizeof(notes) / sizeof((notes)[0]))

void playModeTests() {
  PLAY_PATTERN("Clock mode", MODE_CLOCK);
  PLAY_PATTERN("Timer mode", MODE_TIMER);
  PLAY_PATTERN("AI mode", MODE_AI);
  PLAY_PATTERN("Pet Pomodoro mode", MODE_POMO);
}

void playPetTests() {
  PLAY_PATTERN("Happy pet", PET_HAPPY);
  PLAY_PATTERN("Angry pet", PET_ANGRY);
  PLAY_PATTERN("Sad pet", PET_SAD);
}

void playActionTests() {
  PLAY_PATTERN("Start / resume", START_CUE);
  PLAY_PATTERN("Pause / stop", PAUSE_CUE);
  PLAY_PATTERN("Timer complete", DONE_CUE);
  PLAY_PATTERN("Invalid / error", ERROR_CUE);
}

void playAllTests() {
  Serial.println("\n--- LONG REFERENCE TONE ---");
  PLAY_PATTERN("2-second 1000 Hz tone", LONG_TONE);
  Serial.println("\n--- MODE NOTIFICATIONS ---");
  playModeTests();
  Serial.println("\n--- PET REACTIONS ---");
  playPetTests();
  Serial.println("\n--- TIMER AND SYSTEM CUES ---");
  playActionTests();
  Serial.println("--- TEST COMPLETE ---\n");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  delay(500);

  Serial.println("\nStudyMochi passive-buzzer demo on GPIO 5");
  Serial.println("Commands:");
  Serial.println("  l = one long 2-second tone");
  Serial.println("  a = all sounds");
  Serial.println("  m = mode sounds");
  Serial.println("  p = pet sounds");
  Serial.println("  t = timer/system sounds");

  delay(1000);
  PLAY_PATTERN("2-second 1000 Hz tone", LONG_TONE);
}

void loop() {
  if (!Serial.available()) {
    delay(5);
    return;
  }

  char command = (char)tolower(Serial.read());
  while (Serial.available())
    Serial.read();

  if (command == 'l')
    PLAY_PATTERN("2-second 1000 Hz tone", LONG_TONE);
  else if (command == 'a')
    playAllTests();
  else if (command == 'm')
    playModeTests();
  else if (command == 'p')
    playPetTests();
  else if (command == 't')
    playActionTests();
  else
    Serial.println("Use: l=long, a=all, m=modes, p=pets, t=timer/system");
}
