#pragma once

#include <Arduino.h>
#include <FS.h>
#include <driver/i2s.h>

enum AudioPriority : uint8_t {
  AUDIO_PRIORITY_NONE = 0,
  AUDIO_PRIORITY_PET = 1,
  AUDIO_PRIORITY_MODE = 2,
  AUDIO_PRIORITY_AI = 3,
  AUDIO_PRIORITY_ALARM = 4
};

enum PetSound : uint8_t {
  SOUND_HAPPY,
  SOUND_SAD,
  SOUND_CUDDLE,
  SOUND_ANGRY,
  SOUND_SQUISH,
  SOUND_DIZZY,
  SOUND_TIMER_DONE,
  SOUND_INVALID,
  SOUND_LISTEN_ON,
  SOUND_LISTEN_OFF
};

// Owns the MAX98357A output, microSD WAV playback, generated pet tones,
// and arbitration with Gemini audio. All playback is non-blocking except
// for the short DMA write performed by writeAiPcm().
class AudioManager {
public:
  bool begin(i2s_port_t port = I2S_NUM_1);
  void tick();

  bool playWav(const char *path, AudioPriority priority = AUDIO_PRIORITY_MODE);
  bool playSound(PetSound sound, AudioPriority priority = AUDIO_PRIORITY_PET);

  bool beginAiStream();
  size_t writeAiPcm(uint8_t *pcm, size_t bytes, uint8_t volumePercent,
                    uint8_t *envelope = nullptr);
  void endAiStream();

  void stop();
  bool isBusy() const { return _kind != PLAY_NONE; }
  bool sdReady() const { return _sdReady; }
  AudioPriority priority() const { return _priority; }

private:
  enum PlayKind : uint8_t { PLAY_NONE, PLAY_WAV, PLAY_TONE, PLAY_AI };

  struct ToneStep {
    uint16_t frequency;
    uint16_t durationMs;
    uint16_t amplitude;
  };

  bool claim(AudioPriority priority, PlayKind kind);
  bool openWav(const char *path);
  void tickWav();
  void tickTone();
  void loadTone(PetSound sound);
  void finish();

  i2s_port_t _port = I2S_NUM_1;
  bool _ampReady = false;
  bool _sdReady = false;
  PlayKind _kind = PLAY_NONE;
  AudioPriority _priority = AUDIO_PRIORITY_NONE;

  File _file;
  uint32_t _wavBytesLeft = 0;
  uint32_t _wavSampleRate = 0;
  uint8_t _buffer[512] = {0};
  size_t _bufferSize = 0;
  size_t _bufferOffset = 0;

  ToneStep _tone[10] = {};
  uint8_t _toneCount = 0;
  uint8_t _toneIndex = 0;
  uint32_t _toneSamplesLeft = 0;
  float _tonePhase = 0.0f;
};

extern AudioManager gAudio;
