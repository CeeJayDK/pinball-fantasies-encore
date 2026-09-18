// Speed Devils rules (translated from pfr's src/table/speed.rs and its tasks).
#include "table/Table.h"

namespace pfr {

void Table::speedFrame() {
  SpeedState& s = speed_;
  if (++s.lightPhasePlace == 30) s.lightPhasePlace = 0;
  for (u8 i = 0; i < 3; ++i)
    if (s.timeoutPit[i] != 0 && --s.timeoutPit[i] == 0) lightSet(LightBind::SpeedPit, i, true);
  if (s.timeoutPitAll != 0 && --s.timeoutPitAll == 0) lightSetAll(LightBind::SpeedPit, false);
  if (s.timeoutMilesLeft != 0) --s.timeoutMilesLeft;
  if (s.timeoutMilesRight != 0) --s.timeoutMilesRight;
  if (s.timeoutGearBlink != 0 && --s.timeoutGearBlink == 0) {
    s.curGear = 0;
    lightSetAll(LightBind::SpeedGearNum, false);
    if (!lightState(LightBind::SpeedPitStopHoldBonus, 0)) {
      lightSet(LightBind::SpeedPitStopHoldBonus, 0, true);
      lightBlink(LightBind::SpeedPitStopHoldBonus, 0, 15, s.lightPhasePlace);
    }
  }
  if (s.timeoutJackpot != 0 && --s.timeoutJackpot == 0) lightSet(LightBind::SpeedMiniRampJackpot, 0, false);
}

void Table::speedFlipperPressed() {
  lightRotate(LightBind::SpeedBur);
  speed_.blinkBur = {};
  lightRotate(LightBind::SpeedNin);
  speed_.blinkNin = {};
  if (speed_.timeoutPitAll == 0) {
    speed_.timeoutPit = {};
    lightRotate(LightBind::SpeedPit);
  }
}

void Table::speedDrained() {
  sequencer_->resetPriority();
  effect(EffectBind::Drained);
  sequencer_->resetPriority();
  addTask(TaskKind::DrainSfx);
}

void Table::speedModeCheck() {
  if (modeTimeoutSecs_ != 0) return;
  if (inModeRamp_) playJingleBind(JingleBind::ModeEndRamp);
  if (inModeHit_) playJingleBind(JingleBind::ModeEndHit);
  sequencer_->setMusic(3);
  sequencer_->resetPriority();
  inModeHit_ = false;
  inModeRamp_ = false;
}

void Table::speedHitBur(u8 which) {
  if (speed_.blinkBur[which]) return;
  speed_.blinkBur[which] = true;
  lightSet(LightBind::SpeedBur, which, true);
  modeCountHit();
  scorePremult(Bcd::of("7510"), Bcd::of("550"));
  playSfxBind(SfxBind::SpeedHitTarget);
  if (lightAllLit(LightBind::SpeedBur)) {
    incrJackpot();
    speed_.blinkBur = {true, true, true};
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::SpeedBur, i, 1, 0);
    addTask(TaskKind::SpeedUnblinkBurAll);
    speedGear(2);
  } else {
    lightBlink(LightBind::SpeedBur, which, 1, 0);
    addTask(TaskKind::SpeedUnblinkBur, which);
  }
}

void Table::speedHitNin(u8 which) {
  if (speed_.blinkNin[which]) return;
  speed_.blinkNin[which] = true;
  lightSet(LightBind::SpeedNin, which, true);
  modeCountHit();
  scorePremult(Bcd::of("7510"), Bcd::of("550"));
  playSfxBind(SfxBind::SpeedHitTarget);
  if (lightAllLit(LightBind::SpeedNin)) {
    incrJackpot();
    speed_.blinkNin = {true, true, true};
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::SpeedNin, i, 1, 0);
    addTask(TaskKind::SpeedUnblinkNinAll);
    speedGear(3);
  } else {
    lightBlink(LightBind::SpeedNin, which, 1, 0);
    addTask(TaskKind::SpeedUnblinkNin, which);
  }
}

bool Table::speedGear(u8 which) {
  SpeedState& s = speed_;
  lightSet(LightBind::SpeedGear, which, true);
  lightBlink(LightBind::SpeedGear, which, 1, 0);
  if (!lightAllLit(LightBind::SpeedGear)) return false;
  if (s.maxPlace < 10) {
    lightBlink(LightBind::SpeedPlace, s.maxPlace, 15, s.lightPhasePlace);
    lightBlink(LightBind::SpeedPlace, static_cast<u8>(s.maxPlace + 1), 15, static_cast<u8>((s.lightPhasePlace + 15) % 30));
    s.maxPlace = static_cast<u8>(s.maxPlace + 2);
  } else {
    effect(EffectBind::SpeedExtraGear);
  }
  if (s.curGear < 5) {
    lightSet(LightBind::SpeedGearNum, s.curGear, true);
    ++s.curGear;
  } else {
    s.timeoutGearBlink = 30;
    for (u8 i = 0; i < 6; ++i) lightBlink(LightBind::SpeedGearNum, i, 1, 0);
  }
  lightSetAll(LightBind::SpeedGear, false);
  for (u8 i = 0; i < 4; ++i) lightBlink(LightBind::SpeedGear, i, 2, 0);
  addTask(TaskKind::SpeedUnblinkGearAll);
  effect(EffectBind::SpeedGear);
  return true;
}

void Table::speedGoal() {
  lightSetAll(LightBind::SpeedPlace, false);
  speed_.curPlace = 0;
  speed_.maxPlace = 0;
  if (inMode_)
    addTask(TaskKind::SpeedTurbo);
  else
    speedDoTurbo();
}

void Table::speedDoTurbo() {
  effect(EffectBind::SpeedTurbo);
  inMode_ = true;
  inModeRamp_ = true;
}

void Table::speedOffroad() {
  if (inMode_)
    addTask(TaskKind::SpeedOffroad);
  else
    speedDoOffroad();
}

void Table::speedDoOffroad() {
  playJingleBindForce(JingleBind::SpeedModeHit);
  inMode_ = true;
  inModeHit_ = true;
  startScript(ScriptBind::SpeedModeHit);
}

void Table::speedPitStop() {
  if (lightState(LightBind::SpeedPitStopSuperJackpot, 0)) {
    lightSet(LightBind::SpeedPitStopSuperJackpot, 0, false);
    if (lightState(LightBind::SpeedPitStopGoal, 0)) {
      effect(EffectBind::SpeedSuperJackpotGoal);
      ballTeleportFreeze(Layer::Ground, {256, 41});
      addTask(TaskKind::SpeedPitStop, 150);
      return;
    }
    timerStop_ = true;
    effectForce(EffectBind::SpeedSuperJackpot);
  }
  if (lightState(LightBind::SpeedPitStopGoal, 0)) {
    lightSet(LightBind::SpeedPitStopGoal, 0, false);
    speedGoal();
  }
  if (lightState(LightBind::SpeedPitStopHoldBonus, 0)) {
    lightSet(LightBind::SpeedPitStopHoldBonus, 0, false);
    effect(EffectBind::SpeedHoldBonus);
    holdBonus_ = true;
  }
  ballTeleportFreeze(Layer::Ground, {256, 41});
  addTask(TaskKind::SpeedPitStop, inMode_ ? 80 : 20);
}

void Table::speedCarMod(u8 which) {
  if (!lightState(LightBind::SpeedCarPartLit, which)) return;
  lightSet(LightBind::SpeedCarPartLit, which, false);
  static constexpr EffectBind kCar[5] = {EffectBind::SpeedCar0, EffectBind::SpeedCar1, EffectBind::SpeedCar2,
                                         EffectBind::SpeedCar3, EffectBind::SpeedCar4};
  effect(kCar[which]);
  lightSet(LightBind::SpeedCarPart, which, true);
  lightBlink(LightBind::SpeedCarPart, which, 15,
             (which == 0 || which == 2) ? static_cast<u8>((speed_.lightPhasePlace + 15) % 30) : speed_.lightPhasePlace);
  if (lightAllLit(LightBind::SpeedCarPart)) {
    lightSetAll(LightBind::SpeedCarPart, false);
    for (u8 i = 0; i < 5; ++i) {
      lightBlink(LightBind::SpeedCarPart, i, 1, 0);
      speed_.carMods = 0;
      addTask(TaskKind::SpeedUnblinkCar);
    }
  }
}

void Table::speedRampOffroad() {
  SpeedState& s = speed_;
  if (inModeRamp_) effect(EffectBind::SpeedTurboRamp);
  modeCountRamp();
  effect(EffectBind::SpeedRampOffroad);
  speedCarMod(0);
  speedCarMod(3);
  if (lightState(LightBind::SpeedOffroadMultiBonus, 0) && s.mbActive < 8) {
    static constexpr EffectBind kMb[8] = {EffectBind::SpeedMb2, EffectBind::SpeedMb3, EffectBind::SpeedMb4,
                                          EffectBind::SpeedMb5, EffectBind::SpeedMb6, EffectBind::SpeedMb7,
                                          EffectBind::SpeedMb8, EffectBind::SpeedMb9};
    effect(kMb[s.mbActive]);
    lightSet(LightBind::SpeedBonus, s.mbActive, true);
    ++s.mbActive;
    bonusMultEarly_ = static_cast<u8>(s.mbActive + 1);
    bonusMultLate_ = static_cast<u8>(s.mbActive + 1);
    if (--s.mbPending == 0) lightSet(LightBind::SpeedOffroadMultiBonus, 0, false);
  }
  incrJackpot();
  if (!speedGear(1)) addTask(TaskKind::SpeedUnblinkGear, 1);
}

void Table::speedRampJump() {
  SpeedState& s = speed_;
  if (inModeRamp_) effect(EffectBind::SpeedTurboRamp);
  modeCountRamp();
  incrJackpot();
  if (lightState(LightBind::SpeedMiniRampJackpot, 0)) {
    scoreMain_ += scoreJackpot_;
    scoreJackpot_ = assets_.scoreJackpotInit;
    if (inModeRamp_) {
      effectForce(EffectBind::SpeedJackpot);
      timerStop_ = true;
    } else {
      effect(EffectBind::SpeedJackpot);
    }
    lightSet(LightBind::SpeedMiniRampJackpot, 0, false);
    lightSet(LightBind::SpeedPitStopSuperJackpot, 0, true);
    lightBlink(LightBind::SpeedPitStopSuperJackpot, 0, 15, s.lightPhasePlace);
    addTask(TaskKind::SpeedResetSuperJackpot);
  }
  if (lightState(LightBind::SpeedMiniRampJump, 0)) {
    effect(EffectBind::SpeedJump);
    lightSet(LightBind::SpeedMiniRampJump, 0, false);
  }
  speedCarMod(1);
  if (s.pedalMetal) {
    s.pedalMetal = false;
    effect(EffectBind::SpeedPedalMetal);
    if ((s.curSpeed & 1) == 1 && s.carMods != 5) {
      lightSet(LightBind::SpeedCarPartLit, s.carMods, true);
      lightBlink(LightBind::SpeedCarPartLit, s.carMods, 15, s.lightPhasePlace);
      ++s.carMods;
    }
    if (s.curSpeed < 12) lightSet(LightBind::SpeedSpeed, s.curSpeed, true);
    if (++s.curSpeed == 14) s.curSpeed = 12;
  }
  if (!speedGear(0)) addTask(TaskKind::SpeedUnblinkGear, 0);
  incrJackpot();
}

void Table::speedPitLoop() {
  speedCarMod(2);
  speedCarMod(4);
  if (lightState(LightBind::SpeedPitLoopExtraBall, 0)) {
    effect(EffectBind::SpeedExtraBall);
    lightSet(LightBind::SpeedPitLoopExtraBall, 0, false);
    extraBall();
  }
}

void Table::speedRollPit(u8 which) {
  SpeedState& s = speed_;
  if (s.timeoutPit[which] != 0) return;
  lightSet(LightBind::SpeedPit, which, true);
  modeCountHit();
  playSfxBind(SfxBind::SpeedHitTarget);
  effect(EffectBind::SpeedPit);
  if (lightAllLit(LightBind::SpeedPit) && s.timeoutPitAll == 0) {
    effect(EffectBind::SpeedPitAll);
    if (s.mbActive + s.mbPending < 8) {
      if (s.mbPending == 0) {
        lightSet(LightBind::SpeedOffroadMultiBonus, 0, true);
        lightBlink(LightBind::SpeedOffroadMultiBonus, 0, 10, 0);
      }
      ++s.mbPending;
    } else {
      effect(EffectBind::SpeedMillion);
    }
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::SpeedPit, i, 2, 0);
    s.timeoutPitAll = 40;
  } else {
    lightBlink(LightBind::SpeedPit, which, 1, 0);
    s.timeoutPit[which] = 20;
  }
}

void Table::speedOvertake() {
  SpeedState& s = speed_;
  if (s.curPlace < s.maxPlace) {
    lightSet(LightBind::SpeedPlace, s.curPlace, true);
    ++s.curPlace;
    effect(EffectBind::SpeedOvertake);
    if (s.curPlace == 10) {
      lightSet(LightBind::SpeedPitStopGoal, 0, true);
      lightBlink(LightBind::SpeedPitStopGoal, 0, 15, static_cast<u8>((s.lightPhasePlace + 15) % 30));
      lightSet(LightBind::SpeedMiniRampJackpot, 0, true);
      lightBlink(LightBind::SpeedMiniRampJackpot, 0, 15, s.lightPhasePlace);
      s.timeoutJackpot = 1200;
      effect(EffectBind::SpeedOvertakeFinal);
    }
  }
  effect(EffectBind::SpeedMillion);
  s.pedalMetal = true;
}

void Table::speedBumpMiles() {
  incrJackpot();
  static constexpr EffectBind kMiles[12] = {
      EffectBind::SpeedMiles0, EffectBind::SpeedMiles1, EffectBind::SpeedMiles2,  EffectBind::SpeedMiles3,
      EffectBind::SpeedMiles4, EffectBind::SpeedMiles5, EffectBind::SpeedMiles6,  EffectBind::SpeedMiles7,
      EffectBind::SpeedMiles8, EffectBind::SpeedMiles9, EffectBind::SpeedMiles10, EffectBind::SpeedMiles11};
  effect(kMiles[std::min<int>(speed_.curSpeed, 11)]);
  if (inModeRamp_) effect(EffectBind::SpeedTurboRamp);
  modeCountRamp();
  addCyclone(1);
  numCycloneTarget_ = static_cast<u16>(numCyclone_ / 10 * 10 + 10);
  const int n = numCyclone_, m = numCyclone_ % 20;
  if (n <= 9) {
    effect(EffectBind::SpeedMilesToFirstOffroad);
  } else if (n == 10) {
    speedOffroad();
  } else if (n <= 19) {
    effect(EffectBind::SpeedMilesToExtraBall);
  } else if (n == 20) {
    lightSet(LightBind::SpeedPitLoopExtraBall, 0, true);
    lightBlink(LightBind::SpeedPitLoopExtraBall, 0, 15, 0);
    effect(EffectBind::SpeedMilesExtraBall);
  } else if (m >= 1 && m <= 9) {
    effect(EffectBind::SpeedMilesToJump);
  } else if (m == 10) {
    if (!lightState(LightBind::SpeedMiniRampJump, 0)) {
      lightSet(LightBind::SpeedMiniRampJump, 0, true);
      lightBlink(LightBind::SpeedMiniRampJump, 0, 15, 0);
      effect(EffectBind::SpeedMilesJump);
    }
  } else if (m >= 11) {
    effect(EffectBind::SpeedMilesToOffroad);
  } else {
    speedOffroad();
  }
}

void Table::speedLoadFixup() {
  const SpeedState& s = speed_;
  const u8 other = static_cast<u8>((s.lightPhasePlace + 15) % 30);
  if (lightState(LightBind::SpeedPitStopGoal, 0)) lightBlink(LightBind::SpeedPitStopGoal, 0, 15, other);
  for (u8 i = 0; i < s.curGear; ++i) lightSet(LightBind::SpeedGearNum, i, true);
  for (u8 i = 0; i < 5; ++i)
    if (lightState(LightBind::SpeedCarPart, i))
      lightBlink(LightBind::SpeedCarPart, i, 15, (i == 0 || i == 2) ? other : s.lightPhasePlace);
  for (u8 i = 0; i < 5; ++i)
    if (lightState(LightBind::SpeedCarPartLit, i)) lightBlink(LightBind::SpeedCarPartLit, i, 15, s.lightPhasePlace);
  for (u8 i = 0; i < s.curPlace; ++i) lightSet(LightBind::SpeedPlace, i, true);
  for (u8 i = s.curPlace; i < s.maxPlace; ++i) lightBlink(LightBind::SpeedPlace, i, 15, i % 2 == 0 ? s.lightPhasePlace : other);
  for (u8 i = 0; i < s.curSpeed; ++i)
    if (i < 12) lightSet(LightBind::SpeedSpeed, i, true);
}

bool Table::runSpeedTask(Task& t) {
  SpeedState& s = speed_;
  const u8 which = static_cast<u8>(t.a);
  switch (t.kind) {
    case TaskKind::SpeedUnblinkBur:
      if (s.blinkBur[which]) {
        s.blinkBur[which] = false;
        lightSet(LightBind::SpeedBur, which, true);
      }
      break;
    case TaskKind::SpeedUnblinkBurAll:
      lightSet(LightBind::SpeedGear, 2, true);
      lightSetAll(LightBind::SpeedBur, false);
      s.blinkBur = {};
      break;
    case TaskKind::SpeedUnblinkNin:
      if (s.blinkNin[which]) {
        s.blinkNin[which] = false;
        lightSet(LightBind::SpeedNin, which, true);
      }
      break;
    case TaskKind::SpeedUnblinkNinAll:
      lightSet(LightBind::SpeedGear, 3, true);
      lightSetAll(LightBind::SpeedNin, false);
      s.blinkNin = {};
      break;
    case TaskKind::SpeedUnblinkGear: lightSet(LightBind::SpeedGear, which, true); break;
    case TaskKind::SpeedUnblinkGearAll: lightSetAll(LightBind::SpeedGear, false); break;
    case TaskKind::SpeedOffroad:
      if (!inDrain_) {
        if (inMode_) return true;
        speedDoOffroad();
      }
      break;
    case TaskKind::SpeedTurbo:
      if (!inDrain_) {
        if (inMode_) return true;
        speedDoTurbo();
      }
      break;
    case TaskKind::SpeedPitStop:
      playSfxBind(SfxBind::SpeedEjectPit);
      ballTeleport(Layer::Ground, {256, 41}, {-2100, 800});
      break;
    case TaskKind::SpeedUnblinkCar: lightSetAll(LightBind::SpeedCarPart, false); break;
    case TaskKind::SpeedResetSuperJackpot: lightSet(LightBind::SpeedPitStopSuperJackpot, 0, false); break;
    default: break;
  }
  return false;
}

}  // namespace pfr
