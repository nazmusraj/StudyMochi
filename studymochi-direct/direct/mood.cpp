#include "mood.h"

PetMood gMood;

void PetMood::begin() {
  _score = 35; // Baseline happy
  _lastDecay = millis();
  _tempReaction = FACE_NEUTRAL;
  _reactionUntil = 0;
}

void PetMood::tick(uint32_t now) {
  // Clear temporary emotion override once timer expires
  if (_reactionUntil > 0 && now >= _reactionUntil) {
    _reactionUntil = 0;
    _tempReaction = FACE_NEUTRAL;
  }

  // Decay mood score toward baseline (+35) slowly every 60 seconds
  if (now - _lastDecay >= 60000) {
    _lastDecay = now;
    if (_score > 35) _score--;
    else if (_score < 35) _score++;
  }
}

void PetMood::triggerEvent(MoodEvent evt) {
  uint32_t now = millis();

  switch (evt) {
    case MOOD_EVT_PAT:
      _score += 10;
      if (_score > 100) _score = 100;
      _tempReaction = FACE_HAPPY;
      _reactionUntil = now + 2500;
      break;

    case MOOD_EVT_CUDDLE:
      _score += 25;
      if (_score > 100) _score = 100;
      _tempReaction = FACE_CUDDLE;
      _reactionUntil = now + 3500;
      dfvoicePlay(VOICE_HAPPY_CUDDLE, 1500);
      break;

    case MOOD_EVT_RAPID_TAP:
      _score -= 35;
      if (_score < -100) _score = -100;
      _tempReaction = FACE_ANGRY;
      _reactionUntil = now + 3000;
      dfvoicePlay(VOICE_ANGRY_TAP, 1500);
      break;

    case MOOD_EVT_POMO_COMPLETE:
      _score += 15;
      if (_score > 100) _score = 100;
      _tempReaction = FACE_ECSTATIC;
      _reactionUntil = now + 4000;
      dfvoicePlay(VOICE_POMO_FINISH, 2000);
      break;

    case MOOD_EVT_SHAKE:
      _score -= 15;
      if (_score < -100) _score = -100;
      _tempReaction = FACE_DIZZY;
      _reactionUntil = now + 3000;
      dfvoicePlay(VOICE_SHAKEN_DIZZY, 2000);
      break;

    case MOOD_EVT_TILT_SQUISH:
      _score -= 5;
      if (_score < -100) _score = -100;
      break;
  }
}

FaceState PetMood::currentFace() const {
  if (_reactionUntil > 0 && _tempReaction != FACE_NEUTRAL) {
    return _tempReaction;
  }

  // Base mood expression
  if (_score >= 70)  return FACE_ECSTATIC;
  if (_score >= 20)  return FACE_HAPPY;
  if (_score >= -20) return FACE_IDLE;
  if (_score >= -60) return FACE_SLEEPY;
  return FACE_ANGRY;
}
