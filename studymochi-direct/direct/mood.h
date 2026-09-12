// ════════════════════════════════════════════════════════════════
//   Pet Mood & Emotion Engine for StudyMochi
//
//   Maintains emotional state (-100 to +100), handles reactions,
//   and maps mood to kawaii facial expressions and voice triggers.
// ════════════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
#include "face.h"
#include "dfvoice.h"

enum MoodEvent {
  MOOD_EVT_PAT,           // Single soft tap (+10)
  MOOD_EVT_CUDDLE,        // 2s head hold (+25)
  MOOD_EVT_RAPID_TAP,     // Head tapped repeatedly (-35 -> ANGRY)
  MOOD_EVT_POMO_COMPLETE, // Finished work round (+15)
  MOOD_EVT_SHAKE,         // Shaken violently (-20 -> DIZZY)
  MOOD_EVT_TILT_SQUISH    // Tilted on side (-5)
};

class PetMood {
public:
  void begin();
  void tick(uint32_t now);
  void triggerEvent(MoodEvent evt);

  int  score() const { return _score; }
  FaceState currentFace() const;
  bool isAngry() const { return (_tempReaction == FACE_ANGRY); }
  bool isCuddling() const { return (_tempReaction == FACE_CUDDLE); }
  bool isDizzy() const { return (_tempReaction == FACE_DIZZY); }

private:
  int       _score       = 35; // Baseline happy (range -100 to +100)
  uint32_t  _lastDecay   = 0;
  FaceState _tempReaction = FACE_NEUTRAL;
  uint32_t  _reactionUntil = 0;
};

extern PetMood gMood;
