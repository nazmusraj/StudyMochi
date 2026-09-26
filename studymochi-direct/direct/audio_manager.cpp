#include "audio_manager.h"

#include <SD.h>
#include <SPI.h>
#include <math.h>

#include "config.h"

AudioManager gAudio;

namespace {

struct WavInfo {
  uint16_t encoding = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bits = 0;
  uint32_t dataPosition = 0;
  uint32_t dataSize = 0;
};

uint16_t readLe16(File &file) {
  uint8_t b[2];
  if (file.read(b, sizeof(b)) != sizeof(b)) return 0;
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

uint32_t readLe32(File &file) {
  uint8_t b[4];
  if (file.read(b, sizeof(b)) != sizeof(b)) return 0;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

bool readTag(File &file, const char expected[4]) {
  char tag[4];
  return file.read((uint8_t *)tag, sizeof(tag)) == sizeof(tag) &&
         memcmp(tag, expected, sizeof(tag)) == 0;
}

const char *baseName(const char *path) {
  const char *name = path;
  for (const char *p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') name = p + 1;
  }
  return name;
}

bool sameFileName(const char *path, const char *wanted) {
  const char *a = baseName(path);
  const char *b = baseName(wanted);
  while (*a && *b) {
    char ca = *a >= 'A' && *a <= 'Z' ? *a - 'A' + 'a' : *a;
    char cb = *b >= 'A' && *b <= 'Z' ? *b - 'A' + 'a' : *b;
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == 0 && *b == 0;
}

// The standalone SD test scans the entire card, so it succeeds even when the
// user copied the WAVs to the root or accidentally kept an extra sdcard/
// directory. Preserve the documented /audio path, but find the same filename
// elsewhere as a recovery path.
bool findFileRecursive(File &directory, const char *wanted, File &found,
                       uint8_t depth) {
  if (!directory || !directory.isDirectory()) return false;
  directory.rewindDirectory();

  while (true) {
    File entry = directory.openNextFile();
    if (!entry) break;

    if (entry.isDirectory()) {
      if (depth > 0 && findFileRecursive(entry, wanted, found, depth - 1))
        return true;
    } else if (sameFileName(entry.name(), wanted)) {
      found = entry;
      return true;
    }
    entry.close();
  }
  return false;
}

bool readWavHeader(File &file, WavInfo &wav) {
  if (!file.seek(0) || !readTag(file, "RIFF")) return false;
  (void)readLe32(file);
  if (!readTag(file, "WAVE")) return false;

  bool foundFormat = false;
  bool foundData = false;
  while (file.available()) {
    char chunk[4];
    if (file.read((uint8_t *)chunk, sizeof(chunk)) != sizeof(chunk)) break;
    uint32_t chunkSize = readLe32(file);
    uint32_t chunkStart = file.position();
    uint32_t next = chunkStart + chunkSize + (chunkSize & 1U);

    if (memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16) {
      wav.encoding = readLe16(file);
      wav.channels = readLe16(file);
      wav.sampleRate = readLe32(file);
      (void)readLe32(file);  // Byte rate
      (void)readLe16(file);  // Block alignment
      wav.bits = readLe16(file);
      foundFormat = true;
    } else if (memcmp(chunk, "data", 4) == 0) {
      wav.dataPosition = chunkStart;
      wav.dataSize = chunkSize;
      foundData = true;
    }

    if (foundFormat && foundData)
      return file.seek(wav.dataPosition);
    if (!file.seek(next)) break;
  }
  return false;
}

}  // namespace

bool AudioManager::begin(i2s_port_t port) {
  _port = port;

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SPEAKER_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins;
  memset(&pins, 0xFF, sizeof(pins));
  pins.bck_io_num = PIN_AMP_BCLK;
  pins.ws_io_num = PIN_AMP_LRCLK;
  pins.data_out_num = PIN_AMP_DATA;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  _ampReady = i2s_driver_install(_port, &cfg, 0, nullptr) == ESP_OK &&
              i2s_set_pin(_port, &pins) == ESP_OK;
  if (_ampReady) i2s_zero_dma_buffer(_port);

  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  // Breadboard wiring and some microSD adapters are unreliable at 10 MHz.
  // Start at 4 MHz and retry at the initialization-safe 1 MHz rate.
  _sdReady = SD.begin(PIN_SD_CS, SPI, 4000000);
  if (!_sdReady) {
    SD.end();
    _sdReady = SD.begin(PIN_SD_CS, SPI, 1000000);
  }

  Serial.printf("[audio] amplifier=%s, microSD=%s\n",
                _ampReady ? "ready" : "failed",
                _sdReady ? "ready" : "not found");
  if (_sdReady) {
    const char *required[] = {
        AUDIO_POMO_START, AUDIO_BREAK_START, AUDIO_SESSION_DONE,
        AUDIO_CLOCK_MODE, AUDIO_TIMER_MODE, AUDIO_AI_MODE};
    for (const char *path : required) {
      if (!SD.exists(path))
        Serial.printf("[audio] missing required file: %s\n", path);
    }
  }
  return _ampReady;
}

bool AudioManager::claim(AudioPriority priority, PlayKind kind) {
  if (!_ampReady) return false;
  if (_kind != PLAY_NONE && priority < _priority) return false;
  stop();
  _kind = kind;
  _priority = priority;
  return true;
}

void AudioManager::finish() {
  if (_file) _file.close();
  if (_wavSampleRate != 0 && _wavSampleRate != SPEAKER_SAMPLE_RATE &&
      _ampReady) {
    i2s_set_clk(_port, SPEAKER_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT,
                I2S_CHANNEL_MONO);
  }
  _kind = PLAY_NONE;
  _priority = AUDIO_PRIORITY_NONE;
  _wavBytesLeft = 0;
  _wavSampleRate = 0;
  _bufferSize = 0;
  _bufferOffset = 0;
  _toneCount = 0;
  _toneIndex = 0;
  _toneSamplesLeft = 0;
}

void AudioManager::stop() {
  finish();
  if (_ampReady) i2s_zero_dma_buffer(_port);
}

bool AudioManager::openWav(const char *path) {
  _file = SD.open(path, FILE_READ);
  if (!_file) {
    File root = SD.open("/");
    File found;
    if (findFileRecursive(root, baseName(path), found, 3)) {
      _file = found;
      Serial.printf("[audio] %s not found; using discovered file %s\n", path,
                    _file.name());
    }
    if (root) root.close();
  }

  if (!_file) {
    Serial.printf("[audio] file not found anywhere on card: %s\n", path);
    return false;
  }

  WavInfo wav;
  if (!readWavHeader(_file, wav)) {
    Serial.printf("[audio] invalid RIFF/WAVE header: %s\n", _file.name());
    _file.close();
    return false;
  }
  if (wav.encoding != 1 || wav.channels != 1 || wav.bits != 16) {
    Serial.printf("[audio] unsupported WAV %s: format=%u channels=%u bits=%u\n",
                  _file.name(), wav.encoding, wav.channels, wav.bits);
    _file.close();
    return false;
  }
  if (wav.sampleRate < 8000 || wav.sampleRate > 48000) {
    Serial.printf("[audio] unsupported WAV rate %lu Hz: %s\n",
                  (unsigned long)wav.sampleRate, _file.name());
    _file.close();
    return false;
  }

  if (wav.sampleRate != SPEAKER_SAMPLE_RATE) {
    esp_err_t err = i2s_set_clk(_port, wav.sampleRate,
                                I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    if (err != ESP_OK) {
      Serial.printf("[audio] cannot set %lu Hz I2S clock: %d\n",
                    (unsigned long)wav.sampleRate, (int)err);
      _file.close();
      return false;
    }
  }

  _wavSampleRate = wav.sampleRate;
  _wavBytesLeft = wav.dataSize;
  Serial.printf("[audio] WAV mono PCM, %lu Hz, 16-bit, %lu data bytes\n",
                (unsigned long)wav.sampleRate,
                (unsigned long)wav.dataSize);
  return true;
}

bool AudioManager::playWav(const char *path, AudioPriority priority) {
  if (!_sdReady) {
    Serial.printf("[audio] cannot play %s: microSD is not mounted\n", path);
    return false;
  }
  if (!_ampReady) {
    Serial.printf("[audio] cannot play %s: amplifier/I2S is unavailable\n", path);
    return false;
  }
  if (!claim(priority, PLAY_WAV)) {
    Serial.printf("[audio] cannot play %s: higher-priority audio is active\n",
                  path);
    return false;
  }
  if (!openWav(path)) {
    finish();
    return false;
  }
  Serial.printf("[audio] playing %s\n", path);
  return true;
}

void AudioManager::loadTone(PetSound sound) {
  _toneCount = 0;
  auto add = [&](uint16_t hz, uint16_t ms, uint16_t amplitude) {
    if (_toneCount < 10) _tone[_toneCount++] = {hz, ms, amplitude};
  };

  switch (sound) {
    case SOUND_HAPPY:
      add(740, 65, 6000); add(0, 25, 0); add(990, 90, 7000); break;
    case SOUND_SAD:
      add(370, 150, 4200); add(0, 35, 0); add(277, 240, 3600); break;
    case SOUND_CUDDLE:
      add(220, 130, 2800); add(260, 130, 3000); add(220, 150, 2500); break;
    case SOUND_ANGRY:
      add(210, 90, 6500); add(150, 110, 7500); add(110, 140, 7000); break;
    case SOUND_SQUISH:
      add(460, 45, 5000); add(310, 65, 4000); break;
    case SOUND_DIZZY:
      add(520, 70, 4000); add(330, 70, 4000); add(470, 70, 4000);
      add(280, 100, 3500); break;
    case SOUND_TIMER_DONE:
      add(880, 140, 8500); add(0, 70, 0); add(1175, 180, 8500);
      add(0, 70, 0); add(1568, 260, 9000); break;
    case SOUND_INVALID:
      add(180, 90, 5000); add(0, 50, 0); add(150, 100, 5000); break;
    case SOUND_LISTEN_ON:
      add(660, 55, 4500); add(990, 75, 5000); break;
    case SOUND_LISTEN_OFF:
      add(990, 55, 4500); add(660, 75, 5000); break;
  }
}

bool AudioManager::playSound(PetSound sound, AudioPriority priority) {
  if (!claim(priority, PLAY_TONE)) return false;
  loadTone(sound);
  _toneIndex = 0;
  _tonePhase = 0.0f;
  _toneSamplesLeft = (uint32_t)_tone[0].durationMs *
                     SPEAKER_SAMPLE_RATE / 1000UL;
  return true;
}

void AudioManager::tickWav() {
  if (_bufferOffset >= _bufferSize) {
    if (_wavBytesLeft == 0) {
      finish();
      return;
    }
    size_t wanted = min((uint32_t)sizeof(_buffer), _wavBytesLeft);
    _bufferSize = _file.read(_buffer, wanted);
    _bufferOffset = 0;
    if (_bufferSize == 0) {
      finish();
      return;
    }
    _wavBytesLeft -= _bufferSize;
  }

  size_t written = 0;
  i2s_write(_port, _buffer + _bufferOffset, _bufferSize - _bufferOffset,
            &written, pdMS_TO_TICKS(2));
  _bufferOffset += written;
}

void AudioManager::tickTone() {
  if (_toneIndex >= _toneCount) {
    finish();
    return;
  }
  if (_toneSamplesLeft == 0) {
    _toneIndex++;
    if (_toneIndex >= _toneCount) {
      finish();
      return;
    }
    _toneSamplesLeft = (uint32_t)_tone[_toneIndex].durationMs *
                       SPEAKER_SAMPLE_RATE / 1000UL;
    _tonePhase = 0.0f;
  }

  int16_t pcm[128];
  size_t samples = min((uint32_t)128, _toneSamplesLeft);
  const ToneStep &step = _tone[_toneIndex];
  float phaseStep = 2.0f * PI * step.frequency / SPEAKER_SAMPLE_RATE;
  for (size_t i = 0; i < samples; ++i) {
    pcm[i] = step.frequency == 0 ? 0 :
             (int16_t)(sinf(_tonePhase) * step.amplitude);
    _tonePhase += phaseStep;
    if (_tonePhase >= 2.0f * PI) _tonePhase -= 2.0f * PI;
  }

  size_t written = 0;
  i2s_write(_port, pcm, samples * sizeof(int16_t), &written,
            pdMS_TO_TICKS(2));
  _toneSamplesLeft -= written / sizeof(int16_t);
}

void AudioManager::tick() {
  if (_kind == PLAY_WAV) tickWav();
  else if (_kind == PLAY_TONE) tickTone();
}

bool AudioManager::beginAiStream() {
  return claim(AUDIO_PRIORITY_AI, PLAY_AI);
}

size_t AudioManager::writeAiPcm(uint8_t *pcm, size_t bytes,
                                uint8_t volumePercent, uint8_t *envelope) {
  if (_kind != PLAY_AI && !beginAiStream()) return 0;
  if (volumePercent > 100) volumePercent = 100;

  int16_t *samples = reinterpret_cast<int16_t *>(pcm);
  size_t count = bytes / sizeof(int16_t);
  uint32_t peak = 0;
  for (size_t i = 0; i < count; ++i) {
    int32_t value = (int32_t)samples[i] * volumePercent / 100;
    samples[i] = (int16_t)value;
    uint32_t amplitude = value < 0 ? -value : value;
    if (amplitude > peak) peak = amplitude;
  }
  if (envelope) *envelope = (uint8_t)min((uint32_t)255, peak / 128U);

  size_t written = 0;
  i2s_write(_port, pcm, bytes, &written, pdMS_TO_TICKS(200));
  return written;
}

void AudioManager::endAiStream() {
  if (_kind == PLAY_AI) finish();
}
