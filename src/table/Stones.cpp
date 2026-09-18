// Stones 'n Bones rules (translated from pfr's src/table/stones.rs and its tasks).
#include "table/Table.h"

namespace pfr {

void Table::stonesFrame() {
  StonesState& s = stones_;
  if (++s.lightPhaseRight == 32) s.lightPhaseRight = 0;
  if (++s.lightPhaseTower == 36) s.lightPhaseTower = 0;
  if (inDrain_ || timerStop_) return;
  if (s.timeoutTopLoop != 0) --s.timeoutTopLoop;
  if (s.timeoutLock != 0 && !s.ballLocked) {
    if (--s.timeoutLock == 0) {
      s.lockReady = false;
      if (s.lockVaultReady) {
        s.lockVaultReady = false;
        lightSet(LightBind::StonesVaultLock, 0, false);
      }
      if (s.lockWellReady) {
        s.lockWellReady = false;
        lightSet(LightBind::StonesWellLock, 0, false);
      }
    }
  }
  if (s.timeoutLeftRamp != 0) {
    if (--s.timeoutLeftRamp == 0) {
      if (s.millionPlus) {
        s.millionPlus = false;
        lightSet(LightBind::StonesMillionPlus, 0, false);
      }
      if (s.screamX2) {
        s.screamX2 = false;
        lightSet(LightBind::StonesScreamX2, 0, false);
      }
    }
    if (s.timeoutLeftRamp == 90) {
      if (s.millionPlus) lightBlink(LightBind::StonesMillionPlus, 0, 1, 0);
      if (s.screamX2) lightBlink(LightBind::StonesScreamX2, 0, 1, 0);
    }
  }
  if (s.timeoutMultiBonus != 0) {
    if (--s.timeoutMultiBonus == 0 && s.wellMultiBonus) {
      s.wellMultiBonus = false;
      lightSet(LightBind::StonesWellMultiBonus, 0, false);
    }
    if (s.timeoutMultiBonus == 90 && s.wellMultiBonus) lightBlink(LightBind::StonesWellMultiBonus, 0, 1, 0);
  }
  if (s.timeoutLoopCombo != 0 && --s.timeoutLoopCombo == 0) s.loopCombo = 0;
  if (s.timeoutTowerHunt != 0 && --s.timeoutTowerHunt == 0) {
    s.towerHunt = false;
    s.towerHuntCtr = 0;
    playJingleBind(JingleBind::StonesTowerHuntEnd);
    setMusicMain();
  }
}

void Table::stonesFlipperPressed() {
  if (!stones_.flipperLockKey && !stones_.keySkillshot) lightRotate(LightBind::StonesKey);
  if (!stones_.flipperLockRip) lightRotate(LightBind::StonesRip);
}

void Table::stonesDrained() {
  addTask(TaskKind::DrainSfx);
  lightSet(LightBind::StonesGhost, 7, false);
  sequencer_->resetPriority();
  effect(EffectBind::Drained);
}

void Table::stonesModeCheck() {
  if (modeTimeoutSecs_ != 0) return;
  playJingleBind(inModeHit_ ? JingleBind::ModeEndHit : JingleBind::ModeEndRamp);
  sequencer_->setMusic(3);
  sequencer_->resetPriority();
  inModeHit_ = false;
  inModeRamp_ = false;
}

void Table::stonesStonesBonesAll() {
  incrJackpot();
  stones_.stonesBonesBlinking = true;
  if (!stones_.ghostActive) {
    static constexpr EffectBind kLit[8] = {EffectBind::StonesGhostLit0, EffectBind::StonesGhostLit1,
                                           EffectBind::StonesGhostLit2, EffectBind::StonesGhostLit3,
                                           EffectBind::StonesGhostLit4, EffectBind::StonesGhostLit5,
                                           EffectBind::StonesGhostLit6, EffectBind::StonesGhostLit7};
    effect(kLit[stones_.curGhost]);
    stones_.ghostActive = true;
    lightBlink(LightBind::StonesGhost, stones_.curGhost, 32, 0);
    lightBlink(LightBind::StonesVaultGhost, 0, 18, 0);
  } else {
    effect(EffectBind::StonesStonesBonesAllRedundant);
  }
  for (u8 i = 0; i < 5; ++i) lightBlink(LightBind::StonesStone, i, 2, 0);
  for (u8 i = 0; i < 4; ++i) lightBlink(LightBind::StonesBone, i, 2, 0);
  addTask(TaskKind::StonesUnblinkStonesBones);
}

void Table::stonesHitStone(u8 which) {
  if (stones_.ghostsBlinking || stones_.stoneBlinking[which] || stones_.stonesBonesBlinking) return;
  modeCountHit();
  playSfxBind(SfxBind::StonesHitStone);
  scorePremult(Bcd::of("17520"), Bcd::of("750"));
  lightSet(LightBind::StonesStone, which, true);
  if (lightAllLit(LightBind::StonesStone) && lightAllLit(LightBind::StonesBone)) {
    stonesStonesBonesAll();
  } else {
    stones_.stoneBlinking[which] = true;
    lightBlink(LightBind::StonesStone, which, 2, 0);
    addTask(TaskKind::StonesUnblinkStone, which);
  }
}

void Table::stonesHitBone(u8 which) {
  if (stones_.ghostsBlinking || stones_.boneBlinking[which] || stones_.stonesBonesBlinking) return;
  modeCountHit();
  playSfxBind(SfxBind::StonesHitBone);
  scorePremult(Bcd::of("27530"), Bcd::of("510"));
  lightSet(LightBind::StonesBone, which, true);
  if (lightAllLit(LightBind::StonesStone) && lightAllLit(LightBind::StonesBone)) {
    stonesStonesBonesAll();
  } else {
    stones_.boneBlinking[which] = true;
    lightBlink(LightBind::StonesBone, which, 2, 0);
    addTask(TaskKind::StonesUnblinkBone, which);
  }
}

void Table::stonesRollKeyEntry() {
  raisePhysmap(PhysmapBind::StonesGateRampTower);
  if (stones_.millionPlus) {
    stones_.scoreMillionPlus += Bcd::of("1000000");
    score(stones_.scoreMillionPlus, Bcd::kZero);
    effect(EffectBind::StonesMillionPlus);
    lightSet(LightBind::StonesMillionPlus, 0, false);
    stones_.millionPlus = false;
  }
  stones_.ballLocked = false;
  score(Bcd::of("10000"), Bcd::of("1000"));
}

void Table::stonesRollKey(u8 which) {
  StonesState& s = stones_;
  if (s.keyBlinking) return;
  playSfxBind(SfxBind::RollTrigger);
  scorePremult(Bcd::of("10060"), Bcd::of("1010"));
  lightSet(LightBind::StonesKey, which, true);
  if (s.keySkillshot) {
    const u8 target = *s.keySkillshot;
    if (which == target) {
      s.scoreSkillShot += Bcd::of("1000000");
      scoreMain_ += s.scoreSkillShot;
      effect(EffectBind::StonesSkillShot);
      stonesIncrVault();
      stonesIncrTowerBonus();
      stonesIncrWell();
      incrJackpot();
    } else {
      lightSet(LightBind::StonesKey, target, false);
    }
    s.keySkillshot.reset();
  }
  if (lightAllLit(LightBind::StonesKey)) {
    stonesIncrVault();
    incrJackpot();
    s.keyBlinking = true;
    s.flipperLockKey = true;
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::StonesKey, i, 2, 0);
    auto award = [&](bool& flag, LightBind light) {
      if (!flag) {
        flag = true;
        lightBlink(light, 0, 18, s.lightPhaseTower);
      }
    };
    switch (s.keyTowerCycle) {
      case 0: award(s.tower1m, LightBind::StonesTowerMillion); s.keyTowerCycle = 1; break;
      case 1: award(s.tower5m, LightBind::StonesTower5M); s.keyTowerCycle = 2; break;
      case 2: award(s.towerDoubleBonus, LightBind::StonesTowerDoubleBonus); s.keyTowerCycle = 3; break;
      case 3: award(s.towerHoldBonus, LightBind::StonesTowerHoldBonus); s.keyTowerCycle = 4; break;
      default: award(s.tower5m, LightBind::StonesTower5M); break;
    }
    if (!s.towerOpen) effect(EffectBind::StonesTowerOpen);
    stonesTowerOpen();
    addTask(TaskKind::StonesUnblinkKeyAll);
  } else {
    lightBlink(LightBind::StonesKey, which, 2, 0);
    addTask(TaskKind::StonesUnblinkKey, which);
    s.flipperLockKey = true;
  }
}

void Table::stonesTower() {
  StonesState& s = stones_;
  ballTeleportFreeze(Layer::Overhead, {141, 143});
  s.inTower = true;
  modeCountRamp();
  incrJackpot();
  timerStop_ = true;
  s.towerResumeMode = inMode_;
  s.towerResumeModeRamp = inModeRamp_;
  raisePhysmap(PhysmapBind::StonesGateTowerEntry);
  dropPhysmap(PhysmapBind::StonesGateRampTower);
  s.towerOpen = false;
  lightSet(LightBind::StonesTower, 0, false);
  bool visible = false;
  if (s.towerHunt && s.towerHuntCtr < 3) {
    static constexpr EffectBind kHunt[3] = {EffectBind::StonesTowerHunt0, EffectBind::StonesTowerHunt1,
                                            EffectBind::StonesTowerHunt2};
    visible |= effect(kHunt[s.towerHuntCtr]);
    const u8 music = sequencer_->music();
    if (music != 0x32) sequencer_->setMusic(static_cast<u8>(music + 1));
    ++s.towerHuntCtr;
    stonesTowerOpen();
    silenceEffect_ = true;
  }
  if (s.towerSuperJackpot) {
    s.towerSuperJackpot = false;
    lightSet(LightBind::StonesTowerSuperJackpot, 0, false);
    effectForce(EffectBind::StonesTowerSuperJackpot);
    visible = true;
    silenceEffect_ = true;
  }
  if (s.towerJackpot) {
    s.towerJackpot = false;
    lightSet(LightBind::StonesTowerJackpot, 0, false);
    effectForce(EffectBind::StonesTowerJackpot);
    scoreMain_ += scoreJackpot_;
    scoreJackpot_ = assets_.scoreJackpotInit;
    visible = true;
    silenceEffect_ = true;
    s.towerSuperJackpot = true;
    lightBlink(LightBind::StonesTowerSuperJackpot, 0, 18, s.lightPhaseTower);
    stonesTowerOpen();
    addTask(TaskKind::StonesResetSuperJackpot);
  }
  if (s.towerExtraBall) {
    s.towerExtraBall = false;
    lightSet(LightBind::StonesTowerExtraBall, 0, false);
    visible |= effect(EffectBind::StonesTowerExtraBall);
    extraBall();
    silenceEffect_ = true;
  }
  if (s.towerDoubleBonus) {
    s.towerDoubleBonus = false;
    lightSet(LightBind::StonesTowerDoubleBonus, 0, false);
    visible |= effect(EffectBind::StonesTowerDoubleBonus);
    scoreBonus_ += scoreBonus_;
    silenceEffect_ = true;
  }
  if (s.towerHoldBonus) {
    s.towerHoldBonus = false;
    lightSet(LightBind::StonesTowerHoldBonus, 0, false);
    visible |= effect(EffectBind::StonesTowerHoldBonus);
    holdBonus_ = true;
    silenceEffect_ = true;
  }
  if (s.tower5m) {
    s.tower5m = false;
    lightSet(LightBind::StonesTower5M, 0, false);
    visible |= effect(EffectBind::StonesTower5M);
    silenceEffect_ = true;
  }
  if (s.tower1m) {
    s.tower1m = false;
    lightSet(LightBind::StonesTowerMillion, 0, false);
    visible |= effect(EffectBind::StonesTowerMillion);
    silenceEffect_ = true;
  } else {
    visible |= effect(EffectBind::StonesTowerBonus);
    score(s.scoreTowerBonus, Bcd::kZero);
    s.scoreTowerBonus = Bcd::of("1000000");
    if (s.towerHunt) stonesTowerOpen();
  }
  if (!visible) addTask(TaskKind::StonesTowerEject);
  silenceEffect_ = false;
}

void Table::stonesTowerTilt() {
  ballTeleportFreeze(Layer::Overhead, {141, 143});
  addTask(TaskKind::StonesTowerEject);
}

void Table::stonesEndMode() {
  lightSet(LightBind::StonesTowerJackpot, 0, false);
  lightSet(LightBind::StonesTowerSuperJackpot, 0, false);
  stones_.towerJackpot = false;
  stones_.towerSuperJackpot = false;
  stonesTowerCheckClose();
}

void Table::stonesTowerCheckClose() {
  const StonesState& s = stones_;
  if (!s.towerExtraBall && !s.towerJackpot && !s.towerSuperJackpot && !s.tower1m && !s.tower5m &&
      !s.towerDoubleBonus && !s.towerHoldBonus && !s.towerHunt) {
    stones_.towerOpen = false;
    lightSet(LightBind::StonesTower, 0, false);
    raisePhysmap(PhysmapBind::StonesGateTowerEntry);
  }
}

void Table::stonesTowerOpen() {
  dropPhysmap(PhysmapBind::StonesGateTowerEntry);
  if (!stones_.towerOpen) {
    lightBlink(LightBind::StonesTower, 0, 20, 0);
    stones_.towerOpen = true;
  }
}

void Table::stonesTowerEject() {
  playSfxBind(SfxBind::StonesEject);
  ballTeleport(Layer::Overhead, {141, 143}, {0, -3333});
  stones_.inTower = false;
}

void Table::stonesWell() {
  StonesState& s = stones_;
  if (lightState(LightBind::StonesWellLock, 0)) {
    ball_.frozen = true;
    addTask(TaskKind::StonesWellEject);
    return;
  }
  ballTeleportFreeze(Layer::Ground, {275, 245});
  s.inWell = true;
  modeCountRamp();
  incrJackpot();
  bool visible = false;
  scoreMain_ += s.scoreWell;
  if (s.lockWellReady) {
    s.ballLocked = true;
    sequencer_->resetPriority();
    visible |= effect(EffectBind::StonesLock);
    silenceEffect_ = true;
    ballTeleport(Layer::Ground, {300, 530}, {10, 0});
    specialPlungerEvent_ = true;
    s.inWell = false;
    setMusicPlunger();
    s.lockWellReady = false;
    lightSet(LightBind::StonesWellLock, 0, true);
  }
  visible |= effect(EffectBind::StonesWell);
  if (s.wellMultiBonus) {
    s.wellMultiBonus = false;
    lightSet(LightBind::StonesWellMultiBonus, 0, false);
    const u8 which = lightSequence(LightBind::StonesBonus);
    static constexpr u8 kMult[5] = {2, 4, 6, 8, 10};
    static constexpr EffectBind kMb[5] = {EffectBind::StonesWellMb2, EffectBind::StonesWellMb4,
                                          EffectBind::StonesWellMb6, EffectBind::StonesWellMb8,
                                          EffectBind::StonesWellMb10};
    if (which < 5) {
      bonusMultLate_ = kMult[which];
      visible |= effect(kMb[which]);
    }
  }
  if (!visible) addTask(TaskKind::StonesWellEject);
  silenceEffect_ = false;
}

void Table::stonesWellTilt() { addTask(TaskKind::StonesWellEject); }

void Table::stonesVault() {
  StonesState& s = stones_;
  if (!s.vaultFromRamp) {
    lightSet(LightBind::StonesKickback, 0, false);
    s.kickback = false;
  }
  ballTeleportFreeze(Layer::Ground, {2, 532});
  if (tilted_ || lightState(LightBind::StonesVaultLock, 0)) {
    addTask(TaskKind::StonesVaultEject);
    return;
  }
  s.inVault = true;
  incrJackpot();
  bool visible = false;
  if (s.lockVaultReady) {
    s.ballLocked = true;
    sequencer_->resetPriority();
    visible |= effect(EffectBind::StonesLock);
    silenceEffect_ = true;
    ballTeleport(Layer::Ground, {300, 530}, {10, 0});
    specialPlungerEvent_ = true;
    s.inVault = false;
    setMusicPlunger();
    s.lockVaultReady = false;
    lightSet(LightBind::StonesVaultLock, 0, true);
  }
  if (s.ghostActive) {
    s.ghostActive = false;
    lightSet(LightBind::StonesVaultGhost, 0, false);
    lightSet(LightBind::StonesGhost, s.curGhost, true);
    auto towerJackpot = [&] {
      if (!s.towerJackpot) {
        s.towerJackpot = true;
        lightBlink(LightBind::StonesTowerJackpot, 0, 18, s.lightPhaseTower);
        stonesTowerOpen();
      }
    };
    switch (s.curGhost) {
      case 0: visible |= effect(EffectBind::StonesGhost5M); break;
      case 1:
        visible |= effect(EffectBind::StonesGhostTowerHunt);
        stonesTowerOpen();
        s.towerHunt = true;
        s.towerHuntCtr = 0;
        s.timeoutTowerHunt = 2400;
        sequencer_->setMusic(0x2e);
        break;
      case 2:
        visible |= effect(EffectBind::StonesGhostExtraBall);
        if (!s.towerExtraBall) {
          s.towerExtraBall = true;
          lightBlink(LightBind::StonesTowerExtraBall, 0, 18, s.lightPhaseTower);
          stonesTowerOpen();
        }
        break;
      case 3: visible |= effect(EffectBind::StonesGhost10M); break;
      case 4:
        addTask(TaskKind::StonesModeHit);
        s.vaultHold = true;
        towerJackpot();
        break;
      case 5:
        visible |= effect(EffectBind::StonesGhostDemon);
        s.lockReady = s.lockWellReady = s.lockVaultReady = true;
        s.timeoutLock = 2100;
        s.ballLocked = false;
        s.screamDemon = true;
        lightBlink(LightBind::StonesWellLock, 0, 18, 0);
        lightBlink(LightBind::StonesVaultLock, 0, 18, 0);
        lightBlink(LightBind::StonesScreamDemon, 0, 18, 0);
        break;
      case 6: visible |= effect(EffectBind::StonesGhost15M); break;
      default:
        addTask(TaskKind::StonesModeRamp);
        towerJackpot();
        break;
    }
    if (++s.curGhost == 8) {
      s.curGhost = 0;
      s.ghostsBlinking = true;
      for (u8 i = 0; i < 8; ++i) lightBlink(LightBind::StonesGhost, i, 2, 0);
      addTask(TaskKind::StonesUnblinkGhosts);
    }
  }
  modeCountRamp();
  scoreMain_ += s.scoreVault;
  if (visible) silenceEffect_ = true;
  visible |= effect(EffectBind::StonesVault);
  silenceEffect_ = false;
  if (!visible) addTask(TaskKind::StonesVaultEject);
}

void Table::stonesRampTop() {
  modeCountRamp();
  stonesIncrVault();
  if (stones_.timeoutLoopCombo != 0 && stones_.loopCombo == 2) effect(EffectBind::StonesLoopCombo);
  stones_.loopCombo = 0;
  if (stones_.timeoutTopLoop != 0) {
    effect(EffectBind::StonesTopMillion);
  } else {
    playSfxBind(SfxBind::RollTrigger);
    scorePremult(Bcd::of("10030"), Bcd::of("1020"));
  }
  stones_.timeoutTopLoop = 300;
}

void Table::stonesRollRip(u8 which) {
  StonesState& s = stones_;
  if (s.ripBlinking) return;
  playSfxBind(SfxBind::RollTrigger);
  scorePremult(Bcd::of("10070"), Bcd::of("1080"));
  lightSet(LightBind::StonesRip, which, true);
  if (lightAllLit(LightBind::StonesRip)) {
    s.ripBlinking = true;
    s.flipperLockRip = true;
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::StonesRip, i, 2, 0);
    if (!s.kickback) {
      s.kickback = true;
      lightBlink(LightBind::StonesKickback, 0, 18, 0);
      dropPhysmap(PhysmapBind::StonesGateKickback);
    }
    effect(EffectBind::StonesKickback);
    addTask(TaskKind::StonesUnblinkRipAll);
  } else {
    addTask(TaskKind::StonesUnblinkRip, which);
    lightBlink(LightBind::StonesRip, which, 2, 0);
    s.flipperLockRip = true;
  }
}

void Table::stonesRampScreams() {
  StonesState& s = stones_;
  playSfxBind(SfxBind::RollTrigger);
  scorePremult(Bcd::of("10060"), Bcd::of("1050"));
  if (s.screamDemon) {
    s.screamDemon = false;
    lightSet(LightBind::StonesScreamDemon, 0, false);
    s.timeoutLock = 1;
    const int locked = int{lightState(LightBind::StonesWellLock, 0)} + int{lightState(LightBind::StonesVaultLock, 0)};
    static constexpr EffectBind kDemon[3] = {EffectBind::StonesDemon5M, EffectBind::StonesDemon10M,
                                             EffectBind::StonesDemon20M};
    effect(kDemon[locked]);
    lightSet(LightBind::StonesWellLock, 0, false);
    lightSet(LightBind::StonesVaultLock, 0, false);
    silenceEffect_ = true;
  }
  if (s.timeoutLoopCombo != 0 && s.loopCombo == 1) s.loopCombo = 2;
  modeCountRamp();
  stonesIncrWell();
  addCyclone(1);
  if (s.screamX2) {
    addTask(TaskKind::StonesScreamExtra);
    lightSet(LightBind::StonesScreamX2, 0, false);
    s.screamX2 = false;
  }
  numCycloneTarget_ = static_cast<u16>(numCyclone_ / 10 * 10 + 10);
  if (numCyclone_ % 10 == 0) {
    if (numCyclone_ == 10) {
      if (!s.towerExtraBall) {
        s.towerExtraBall = true;
        lightBlink(LightBind::StonesTowerExtraBall, 0, 18, s.lightPhaseTower);
        stonesTowerOpen();
        effect(EffectBind::StonesScreamsExtraBall);
      }
    } else {
      if (!s.tower5m) {
        s.tower5m = true;
        lightBlink(LightBind::StonesTower5M, 0, 18, s.lightPhaseTower);
        stonesTowerOpen();
        effect(EffectBind::StonesTowerOpen);
      }
      effect(EffectBind::StonesScreamsTo5M);
    }
  } else if (numCyclone_ < 10) {
    effect(EffectBind::StonesScreamsToExtraBall);
  } else {
    effect(EffectBind::StonesScreamsTo5M);
  }
  silenceEffect_ = false;
}

void Table::stonesRampLeftToLane() {
  StonesState& s = stones_;
  dropPhysmap(PhysmapBind::StonesGateRampLeft0);
  if (tilted_) return;
  modeCountRamp();
  stonesIncrVault();
  playSfxBind(SfxBind::RollTrigger);
  scorePremult(Bcd::of("10030"), Bcd::of("1040"));
  if (!s.millionPlus) {
    s.millionPlus = true;
    lightBlink(LightBind::StonesMillionPlus, 0, 16, s.lightPhaseRight);
  }
  if (!s.screamX2) {
    s.screamX2 = true;
    lightBlink(LightBind::StonesScreamX2, 0, 16, s.lightPhaseRight);
  }
  s.timeoutLeftRamp = 450;
  if (!s.wellMultiBonus && !lightAllLit(LightBind::StonesBonus)) {
    s.wellMultiBonus = true;
    lightBlink(LightBind::StonesWellMultiBonus, 0, 16, s.lightPhaseRight);
  }
  s.timeoutMultiBonus = 570;
  s.loopCombo = 1;
  s.timeoutLoopCombo = hifps_ ? 936 : 780;
}

void Table::stonesRampLeftToVault() {
  raisePhysmap(PhysmapBind::StonesGateRampLeft0);
  if (tilted_) return;
  modeCountRamp();
  playSfxBind(SfxBind::RollTrigger);
  scorePremult(Bcd::of("10020"), Bcd::of("1010"));
}

void Table::stonesIncrVault() { stones_.scoreVault += Bcd::of("82150"); }
void Table::stonesIncrWell() { stones_.scoreWell += Bcd::of("64190"); }
void Table::stonesIncrTowerBonus() { stones_.scoreTowerBonus += Bcd::of("223470"); }

void Table::stonesLoadFixup() {
  if (stones_.kickback) {
    lightBlink(LightBind::StonesKickback, 0, 18, 0);
    dropPhysmap(PhysmapBind::StonesGateKickback);
  }
  for (u8 i = 0; i < stones_.curGhost; ++i) lightSet(LightBind::StonesGhost, i, true);
  if (stones_.ghostActive) {
    lightBlink(LightBind::StonesGhost, stones_.curGhost, 32, 0);
    lightBlink(LightBind::StonesVaultGhost, 0, 18, 0);
  }
}

bool Table::runStonesTask(Task& t) {
  StonesState& s = stones_;
  const u8 which = static_cast<u8>(t.a);
  switch (t.kind) {
    case TaskKind::StonesUnblinkStone:
      if (!s.stonesBonesBlinking) lightSet(LightBind::StonesStone, which, true);
      s.stoneBlinking[which] = false;
      break;
    case TaskKind::StonesUnblinkBone:
      if (!s.stonesBonesBlinking) lightSet(LightBind::StonesBone, which, true);
      s.boneBlinking[which] = false;
      break;
    case TaskKind::StonesUnblinkStonesBones:
      lightSetAll(LightBind::StonesStone, false);
      lightSetAll(LightBind::StonesBone, false);
      s.stonesBonesBlinking = false;
      break;
    case TaskKind::StonesUnblinkKey:
      if (!s.keyBlinking) {
        lightSet(LightBind::StonesKey, which, true);
        s.flipperLockKey = false;
      }
      break;
    case TaskKind::StonesUnblinkKeyAll:
      lightSetAll(LightBind::StonesKey, false);
      s.keyBlinking = false;
      s.flipperLockKey = false;
      break;
    case TaskKind::StonesResetSuperJackpot:
      if (s.towerSuperJackpot) {
        s.towerSuperJackpot = false;
        lightSet(LightBind::StonesTowerSuperJackpot, 0, false);
        stonesTowerCheckClose();
      }
      break;
    case TaskKind::StonesTowerEject:
      timerStop_ = false;
      if (s.towerResumeMode)
        startScript(s.towerResumeModeRamp ? ScriptBind::StonesModeRampContinue : ScriptBind::StonesModeHitContinue);
      stonesTowerEject();
      break;
    case TaskKind::StonesTowerEjectNow: stonesTowerEject(); break;
    case TaskKind::StonesWellEject:
      playSfxBind(SfxBind::StonesEject);
      ballTeleport(Layer::Ground, {275, 245}, {-666, 1666});
      s.inWell = false;
      break;
    case TaskKind::StonesVaultEject:
      playSfxBind(SfxBind::StonesEject);
      dropPhysmap(PhysmapBind::StonesGateKickback);
      ballTeleport(Layer::Ground, {2, 532}, {0, -2880});
      s.inVault = false;
      addTask(TaskKind::StonesRaiseKickback);
      break;
    case TaskKind::StonesUnblinkGhosts:
      lightSetAll(LightBind::StonesGhost, false);
      s.ghostsBlinking = false;
      break;
    case TaskKind::StonesModeHit:
      if (inMode_) return true;
      if (!inDrain_) {
        if (s.inVault) s.vaultHold = true;
        effect(EffectBind::StonesGhostGhostHunter);
      }
      break;
    case TaskKind::StonesModeRamp:
      if (inMode_) return true;
      if (!inDrain_) {
        s.vaultHold = true;
        effect(EffectBind::StonesGhostGrimReaper);
      }
      break;
    case TaskKind::StonesRaiseKickback:
      if (!s.kickback) raisePhysmap(PhysmapBind::StonesGateKickback);
      s.vaultFromRamp = false;
      break;
    case TaskKind::StonesUnblinkRip:
      if (!s.ripBlinking) {
        lightSet(LightBind::StonesRip, which, true);
        s.flipperLockRip = false;
      }
      break;
    case TaskKind::StonesUnblinkRipAll:
      lightSetAll(LightBind::StonesRip, false);
      s.ripBlinking = false;
      s.flipperLockRip = false;
      break;
    case TaskKind::StonesScreamExtra: stonesRampScreams(); break;
    default: break;
  }
  return false;
}

}  // namespace pfr
