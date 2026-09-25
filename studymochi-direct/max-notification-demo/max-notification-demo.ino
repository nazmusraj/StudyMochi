#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// Existing StudyMochi MAX98357A wiring.
constexpr i2s_port_t I2S_SPEAKER = I2S_NUM_1;
constexpr int PIN_BCLK = 26;
constexpr int PIN_LRC  = 27;
constexpr int PIN_DIN  = 14;

constexpr uint32_t SAMPLE_RATE = 24000;
// About 17% of full scale. Increase carefully; 12000 is already quite loud.
constexpr int16_t VOLUME = 5500;

struct Note {
  uint16_t hz;
  uint16_t ms;
  uint16_t gapMs;
};

// Long, easy-to-hear test patterns. Each tone lasts hundreds of milliseconds.
const Note MODE_CLOCK[] = {{900, 350, 120}, {1200, 500, 0}};
const Note MODE_TIMER[] = {{1250, 350, 120}, {1250, 500, 0}};
const Note MODE_AI[]    = {{1200, 350, 120}, {1750, 500, 0}};
const Note MODE_POMO[]  = {
    {1000, 300, 100}, {1400, 300, 100}, {1900, 500, 0}};

const Note PET_HAPPY[] = {{740, 350, 120}, {990, 600, 0}};
const Note PET_ANGRY[] = {
    {210, 350, 100}, {150, 450, 100}, {110, 600, 0}};
const Note PET_SAD[] = {{370, 500, 150}, {277, 800, 0}};

const Note START_CUE[] = {{700, 350, 120}, {1100, 550, 0}};
const Note PAUSE_CUE[] = {{1100, 350, 120}, {700, 550, 0}};
const Note DONE_CUE[] = {
    {880, 400, 150}, {1175, 450, 150}, {1568, 800, 0}};
const Note ERROR_CUE[] = {{180, 450, 180}, {150, 650, 0}};

bool beginSpeaker() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_BCLK;
  pins.ws_io_num = PIN_LRC;
  pins.data_out_num = PIN_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(I2S_SPEAKER, &config, 0, nullptr) != ESP_OK)
    return false;
  if (i2s_set_pin(I2S_SPEAKER, &pins) != ESP_OK)
    return false;
  i2s_zero_dma_buffer(I2S_SPEAKER);
  return true;
}

void playTone(uint16_t frequency, uint16_t durationMs) {
  constexpr size_t BLOCK = 128;
  int16_t pcm[BLOCK];
  uint32_t totalSamples = (uint32_t)durationMs * SAMPLE_RATE / 1000UL;
  uint32_t produced = 0;
  float phase = 0.0f;
  float phaseStep = 2.0f * PI * frequency / SAMPLE_RATE;
  const uint32_t fadeSamples = SAMPLE_RATE / 200; // 5 ms click-free fade

  while (produced < totalSamples) {
    size_t count = min((uint32_t)BLOCK, totalSamples - produced);
    for (size_t i = 0; i < count; ++i) {
      uint32_t position = produced + i;
      uint32_t fromEnd = totalSamples - position;
      float envelope = 1.0f;
      if (position < fadeSamples)
        envelope = (float)position / fadeSamples;
      if (fromEnd < fadeSamples)
        envelope = min(envelope, (float)fromEnd / fadeSamples);

      pcm[i] = (int16_t)(sinf(phase) * VOLUME * envelope);
      phase += phaseStep;
      if (phase >= 2.0f * PI)
        phase -= 2.0f * PI;
    }

    size_t written = 0;
    i2s_write(I2S_SPEAKER, pcm, count * sizeof(int16_t), &written,
              portMAX_DELAY);
    produced += written / sizeof(int16_t);
  }
  i2s_zero_dma_buffer(I2S_SPEAKER);
}

void playPattern(const char *name, const Note *notes, size_t noteCount) {
  Serial.printf("Playing: %s\n", name);
  for (size_t i = 0; i < noteCount; ++i) {
    const Note &note = notes[i];
    playTone(note.hz, note.ms);
    if (note.gapMs)
      delay(note.gapMs);
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
  delay(500);
  Serial.println("\nStudyMochi MAX98357A notification demo");
  Serial.println("BCLK=26, LRC=27, DIN=14");

  if (!beginSpeaker()) {
    Serial.println("ERROR: I2S/MAX98357A initialization failed.");
    while (true)
      delay(1000);
  }

  Serial.println("Commands: a=all, m=modes, p=pets, t=timer/system");
  delay(1000);
  playAllTests();
}

void loop() {
  if (!Serial.available()) {
    delay(5);
    return;
  }

  char command = (char)tolower(Serial.read());
  while (Serial.available())
    Serial.read();

  if (command == 'a')
    playAllTests();
  else if (command == 'm')
    playModeTests();
  else if (command == 'p')
    playPetTests();
  else if (command == 't')
    playActionTests();
  else
    Serial.println("Use: a=all, m=modes, p=pets, t=timer/system");
}
