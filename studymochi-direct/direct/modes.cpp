#include "modes.h"

ModeManager gModes;

void ModeManager::begin() {
  _mainMode = MODE_CLOCK;
  _subMode = SUB_CLOCK_TIME;
  applyScreen();
}

void ModeManager::setMainMode(MainMode m) {
  if (m >= MAIN_MODE_COUNT) m = MODE_CLOCK;
  _mainMode = m;
  _subMode = 0;

  if (_mainMode == MODE_CLOCK)     dfvoicePlay(VOICE_MODE_CLOCK, 1000);
  else if (_mainMode == MODE_TIMER) dfvoicePlay(VOICE_MODE_TIMER, 1000);
  else if (_mainMode == MODE_AI)    dfvoicePlay(VOICE_MODE_AI, 1000);

  applyScreen();
}

void ModeManager::nextMainMode() {
  setMainMode((MainMode)((_mainMode + 1) % MAIN_MODE_COUNT));
}

void ModeManager::nextSubMode() {
  if (_mainMode == MODE_CLOCK) {
    _subMode = (_subMode + 1) % SUB_CLOCK_COUNT;
  } else if (_mainMode == MODE_TIMER) {
    _subMode = (_subMode + 1) % SUB_TIMER_COUNT;
  }
  applyScreen();
}

void ModeManager::applyScreen() {
  switch (_mainMode) {
    case MODE_CLOCK:
      if (_subMode == SUB_CLOCK_WEATHER) faceSetScreen(SCR_WEATHER);
      else                               faceSetScreen(SCR_CLOCK);
      break;

    case MODE_TIMER:
      if (_subMode == SUB_TIMER_POMO)         faceSetScreen(SCR_POMO);
      else if (_subMode == SUB_TIMER_CUSTOM)   faceSetScreen(SCR_TIMER);
      else                                     faceSetScreen(SCR_STOPWATCH);
      break;

    case MODE_AI:
      faceSetScreen(SCR_FACE);
      break;

    default:
      faceSetScreen(SCR_CLOCK);
      break;
  }
}

void ModeManager::update(uint32_t now, Touch &topTouch, Touch &sideTouch) {
  // ── Side Touch Navigation ──
  // Long press (2s) = Cycle Main Mode
  if (sideTouch.tookHold()) {
    nextMainMode();
    return;
  }
  // Short tap = Cycle Sub Mode
  if (sideTouch.tookTap()) {
    nextSubMode();
    return;
  }

  // ── Top Touch & Orientation ──
  handleTopTouch(topTouch);
  handleOrientation();

  // Sync active face state with mood when idle in Clock/Pet mode
  if (_mainMode == MODE_CLOCK && faceGetState() != FACE_BOOT && faceGetState() != FACE_PORTAL) {
    FaceState targetFace = gMood.currentFace();
    if (targetFace != faceGetState() &&
        faceGetState() != FACE_LISTENING &&
        faceGetState() != FACE_THINKING &&
        faceGetState() != FACE_SPEAKING) {
      faceSetState(targetFace);
    }
  }
}

void ModeManager::handleTopTouch(Touch &topTouch) {
  // 1. Check for rapid-tap anger event (3 taps in 1.5s)
  if (topTouch.tookRapidTap(3, 1500)) {
    gMood.triggerEvent(MOOD_EVT_RAPID_TAP);
    faceSetState(FACE_ANGRY);
    return;
  }

  // 2. Mode-specific handling
  if (_mainMode == MODE_CLOCK) {
    if (topTouch.tookHold()) {
      gMood.triggerEvent(MOOD_EVT_CUDDLE);
      faceSetState(FACE_CUDDLE);
    } else if (topTouch.tookTap()) {
      gMood.triggerEvent(MOOD_EVT_PAT);
      faceSetState(FACE_HAPPY);
    }
  } else if (_mainMode == MODE_TIMER) {
    if (_subMode == SUB_TIMER_POMO) {
      if (topTouch.tookTap()) {
        gPomodoro.toggle();
        if (gPomodoro.isRunning()) dfvoicePlay(VOICE_POMO_START, 1200);
      } else if (topTouch.tookHold()) {
        gPomodoro.nextProfile();
      }
    } else if (_subMode == SUB_TIMER_STOPWATCH) {
      if (topTouch.tookTap()) {
        gStopwatch.toggle();
      } else if (topTouch.tookHold()) {
        gStopwatch.reset();
      }
    }
  }
}

void ModeManager::handleOrientation() {
  OrientFace newFace;
  bool changed = imuTookOrientationChange(newFace);

  // 1. Pomodoro 4-Orientation Switching
  if (_mainMode == MODE_TIMER && _subMode == SUB_TIMER_POMO) {
    if (changed) {
      int targetPreset = imuGetPomoPresetIndex();
      if (targetPreset != gPomodoro.currentProfileIndex()) {
        gPomodoro.selectProfile(targetPreset);
      }
    }
  }

  // 2. Clock Mode Face Squish Physics & Vocal Protest
  if (_mainMode == MODE_CLOCK || faceScreen() == SCR_FACE) {
    int offX = imuSquishOffsetX();
    int offY = imuSquishOffsetY();
    faceSetSquish(offX, offY);

    if (offX < -18) {
      dfvoicePlay(VOICE_SQUISH_LEFT, 2500);
      gMood.triggerEvent(MOOD_EVT_TILT_SQUISH);
    } else if (offX > 18) {
      dfvoicePlay(VOICE_SQUISH_RIGHT, 2500);
      gMood.triggerEvent(MOOD_EVT_TILT_SQUISH);
    }
  } else {
    faceSetSquish(0, 0);
  }

  // 3. Shaken / Dizzy Detection
  if (imuTookShake()) {
    gMood.triggerEvent(MOOD_EVT_SHAKE);
    faceSetState(FACE_DIZZY);
  }
}
