// Party Land rules (translated from pfr's src/table/party.rs and its tasks).
#include "table/Table.h"

namespace pfr {

void Table::partyFrame() {
  PartyState& p = party_;
  if (inDrain_) return;
  if (p.timeoutSkillShot != 0) --p.timeoutSkillShot;
  if (p.timeoutPartyT != 0) --p.timeoutPartyT;
  if (p.timeoutPartyPr != 0) --p.timeoutPartyPr;
  if (p.timeoutSpringLoop != 0) --p.timeoutSpringLoop;
  if (p.timeoutTunnel != 0) {
    --p.timeoutTunnel;
    if (p.timeoutTunnel == 720) {
      lightSet(LightBind::PartyTunnel, 2, false);
      lightSet(LightBind::PartyTunnel, 1, false);
      lightBlink(LightBind::PartyTunnel, 1, 8, 0);
    } else if (p.timeoutTunnel == 0) {
      lightSet(LightBind::PartyTunnel, 1, false);
      lightSet(LightBind::PartyTunnel, 0, false);
      lightBlink(LightBind::PartyTunnel, 0, 8, 0);
    }
  }
  if (++p.lightPhaseSnack == 16) p.lightPhaseSnack = 0;
  if (++p.lightPhaseOrbitSpecial == 24) p.lightPhaseOrbitSpecial = 0;
  if (++p.lightPhasePuke == 4) p.lightPhasePuke = 0;
  if (++p.lightPhaseDemon == 28) p.lightPhaseDemon = 0;
}

void Table::partyFlipperPressed() {
  if (!party_.flipperLockPuke) lightRotate(LightBind::PartyPuke);
}

void Table::partyModeCheck() {
  PartyState& p = party_;
  if (modeTimeoutSecs_ == 2 && p.demonJackpotTimed && !p.demonJackpot) lightBlink(LightBind::PartyDemonJackpot, 0, 2, 0);
  if (modeTimeoutSecs_ != 0) return;
  inMode_ = false;
  p.demonJackpotTimed = false;
  if (!p.demonJackpot) lightSet(LightBind::PartyDemonJackpot, 0, false);
  if (inModeHit_) {
    inModeHit_ = false;
    lightSet(LightBind::PartyHappyHour, 0, false);
    effect(EffectBind::PartyHappyHourEnd);
    if (pendingModeRamp_) {
      pendingModeRamp_ = false;
      addTask(TaskKind::PartyMegaLaugh);
    }
  } else {
    inModeRamp_ = false;
    lightSet(LightBind::PartyMegaLaugh, 0, false);
    effect(EffectBind::PartyMegaLaughEnd);
    if (pendingModeHit_) {
      pendingModeHit_ = false;
      addTask(TaskKind::PartyHappyHour);
    }
  }
  sequencer_->setMusic(1);
}

void Table::partyDrained() {
  if (ballScoredPoints_) {
    sequencer_->resetPriority();
    effect(EffectBind::Drained);
    setMusicSilence();
    addTask(TaskKind::DrainSfx);
  } else {
    startScript(ScriptBind::PartyOn);
    playJinglePlunger();
    partyOn_ = true;
    addTask(TaskKind::PartyOn);
  }
}

void Table::partyStartDropZoneScroll() {
  addTask(TaskKind::PartyDropZoneScroll, scroll_.pos);
  scroll_.setSpecialTargetNow(scroll_.pos);
}

void Table::partyStartDropZone() {
  ballTeleport(Layer::Ground, {15, 47}, {0, 0});
  addTask(TaskKind::PartyDropZoneWait);
  partyStartDropZoneScroll();
}

void Table::partyParty(u8 which) {
  if (lightState(LightBind::PartyParty, which)) return;
  lightSet(LightBind::PartyParty, which, true);
  static constexpr EffectBind kLetters[5] = {EffectBind::PartyPartyP, EffectBind::PartyPartyA, EffectBind::PartyPartyR,
                                             EffectBind::PartyPartyT, EffectBind::PartyPartyY};
  effect(kLetters[which]);
  partyCheckPartyAll();
}

void Table::partyCheckPartyAll() {
  if (!lightAllLit(LightBind::PartyParty)) return;
  if (inMode_)
    pendingModeHit_ = true;
  else
    partyHappyHour();
}

void Table::partyHappyHour() {
  effect(EffectBind::PartyHappyHour);
  sequencer_->setMusic(0x2b);
  lightSetAll(LightBind::PartyParty, false);
  if (!party_.demonJackpot && !party_.demonJackpotTimed)
    lightBlink(LightBind::PartyDemonJackpot, 0, 14, party_.lightPhaseDemon);
  party_.demonJackpotTimed = true;
  inMode_ = true;
  inModeHit_ = true;
  lightBlink(LightBind::PartyHappyHour, 0, 8, 0);
  pendingMode_ = true;
}

bool Table::partyCrazyLetter(EffectBind e) {
  incrJackpot();
  const bool res = effect(e);
  lightSequence(LightBind::PartyCrazy);
  if (lightAllLit(LightBind::PartyCrazy)) {
    lightSetAll(LightBind::PartyCrazy, false);
    if (inMode_)
      pendingModeRamp_ = true;
    else
      partyMegaLaugh();
  }
  return res;
}

void Table::partyMegaLaugh() {
  effect(EffectBind::PartyMegaLaugh);
  sequencer_->setMusic(0x19);
  if (!party_.demonJackpot && !party_.demonJackpotTimed)
    lightBlink(LightBind::PartyDemonJackpot, 0, 14, party_.lightPhaseDemon);
  party_.demonJackpotTimed = true;
  inMode_ = true;
  inModeRamp_ = true;
  lightBlink(LightBind::PartyMegaLaugh, 0, 8, 0);
  pendingMode_ = true;
}

void Table::partyArcadeButton() {
  if (party_.arcadeButtonJustHit) return;
  playSfxBind(SfxBind::PartyArcadeButton);
  party_.arcadeButtonJustHit = true;
  addTask(TaskKind::PartyResetArcadeButton);
  if (!party_.arcadeOpen) {
    party_.arcadeOpen = true;
    lightBlink(LightBind::PartyArcade, 0, 12, 0);
    lightBlink(LightBind::PartyArcade, 1, 12, 0);
  }
}

void Table::partyHitDuck(u8 which) {
  PartyState& p = party_;
  if (!lightState(LightBind::PartyDuckDrop, which)) return;
  if (p.duckHit[which]) return;
  p.duckHit[which] = true;
  lightSet(LightBind::PartyDuckDrop, which, false);
  addTask(TaskKind::PartyDuckDrop, which);
  playSfxBind(SfxBind::PartyHitDuck);
  scorePremult(Bcd::of("7510"), Bcd::of("750"));
  modeCountHit();
  if (lightAllUnlit(LightBind::PartyDuckDrop)) {
    effect(EffectBind::PartyDuckAll);
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::PartyDuck, i, 2, 0);
    addTask(TaskKind::PartyDuckAllUnblink);
    if (!p.snackLit[p.curSnack]) {
      const u8 phase = p.curSnack == 1 ? static_cast<u8>((p.lightPhaseSnack + 8) % 16) : p.lightPhaseSnack;
      lightBlink(LightBind::PartySnack, p.curSnack, 8, phase);
      p.snackLit[p.curSnack] = true;
    }
    if (++p.curSnack == 3) p.curSnack = 0;
  } else {
    addTask(TaskKind::PartyDuckUnblink, which);
    lightBlink(LightBind::PartyDuck, which, 3, 0);
  }
}

void Table::partyOrbitRight() {
  PartyState& p = party_;
  if (p.orbitRightBlinking) return;
  incrJackpot();
  modeCountRamp();
  if (p.timeoutPartyT != 0) partyParty(3);
  p.timeoutPartyT = 600;
  p.timeoutPartyPr = 300;
  if (p.orbitRightCycle < 2) {
    lightSet(LightBind::PartyRightOrbitScore, p.orbitRightCycle, true);
    lightBlink(LightBind::PartyRightOrbitScore, static_cast<u8>(p.orbitRightCycle + 1), 9, 0);
    effect(p.orbitRightCycle == 0 ? EffectBind::PartyOrbit250k : EffectBind::PartyOrbit500k);
    ++p.orbitRightCycle;
  } else {
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::PartyRightOrbitScore, i, 2, 0);
    effect(EffectBind::PartyOrbit750k);
    addTask(TaskKind::PartyOrbitRightUnblink);
    p.orbitRightBlinking = true;
    p.orbitRightCycle = 0;
  }
  if (p.orbitRightMb) {
    p.orbitRightMb = false;
    lightSet(LightBind::PartyRightOrbitMultiBonus, 0, false);
    static constexpr u8 kMult[4] = {2, 4, 6, 8};
    static constexpr EffectBind kMb[4] = {EffectBind::PartyOrbitMb2, EffectBind::PartyOrbitMb4,
                                          EffectBind::PartyOrbitMb6, EffectBind::PartyOrbitMb8};
    const u8 which = lightSequence(LightBind::PartyBonus);
    if (which < 4) {
      effect(kMb[which]);
      bonusMultEarly_ = kMult[which];
      bonusMultLate_ = kMult[which];
    }
  }
  if (p.orbitRightHb) {
    p.orbitRightHb = false;
    lightSet(LightBind::PartyRightOrbitHoldBonus, 0, false);
    effect(EffectBind::PartyOrbitHoldBonus);
    holdBonus_ = true;
  }
  if (p.orbitRightDb) {
    p.orbitRightDb = false;
    lightSet(LightBind::PartyRightOrbitDoubleBonus, 0, false);
    effect(EffectBind::PartyOrbitDoubleBonus);
    scoreBonus_ += scoreBonus_;
  }
}

void Table::partyOrbitLeft() {
  PartyState& p = party_;
  if (p.madBlinking) return;
  incrJackpot();
  modeCountRamp();
  if (p.timeoutPartyT != 0) partyParty(3);
  p.timeoutPartyT = 600;
  const u8 which = lightSequence(LightBind::PartyMad);
  static constexpr EffectBind kMad[3] = {EffectBind::PartyOrbitMad0, EffectBind::PartyOrbitMad1,
                                         EffectBind::PartyOrbitMad2};
  if (which < 3) effect(kMad[which]);
  if (which < 2) {
    lightBlink(LightBind::PartyMad, which, 2, 0);
    addTask(TaskKind::PartyMadUnblink, which);
  } else {
    for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::PartyMad, i, 2, 0);
    addTask(TaskKind::PartyMadAllUnblink);
    p.madBlinking = true;
    partyCrazyLetter(EffectBind::PartyOrbitCrazy);
  }
}

void Table::partySecret() {
  party_.secretDropRelease = false;
  effect(EffectBind::PartySecret);
  incrJackpot();
  lightBlink(LightBind::PartyCycloneX5, 0, 6, 0);
  party_.cycloneX5 = true;
  addTask(TaskKind::PartySecretDrop);
  ballTeleport(Layer::Ground, {15, 47}, {0, 0});
}

void Table::partySecretTilt() {
  ballTeleport(Layer::Ground, {15, 47}, {0, 0});
  partyStartDropZone();
}

void Table::partyTunnel() {
  PartyState& p = party_;
  incrJackpot();
  modeCountRamp();
  if (p.timeoutSkillShot != 0) {
    incrJackpot();
    p.timeoutSkillShot = 0;
    p.scoreTunnelSkillShot += Bcd::of("1000000");
    scoreMain_ += p.scoreTunnelSkillShot;
    effect(EffectBind::PartyTunnelSkillShot);
    silenceEffect_ = true;
    partyParty(0);
  } else if (p.timeoutPartyPr != 0) {
    partyParty(0);
  }
  if (!lightState(LightBind::PartyTunnel, 0)) {
    effect(EffectBind::PartyTunnel1M);
    p.timeoutTunnel = 720;
    lightSet(LightBind::PartyTunnel, 0, true);
    lightBlink(LightBind::PartyTunnel, 1, 8, 0);
  } else if (!lightState(LightBind::PartyTunnel, 1)) {
    effect(EffectBind::PartyTunnel3M);
    p.timeoutTunnel = 1440;
    lightSet(LightBind::PartyTunnel, 1, true);
    lightBlink(LightBind::PartyTunnel, 2, 8, 0);
  } else {
    effect(EffectBind::PartyTunnel5M);
    p.timeoutTunnel = 1440;
  }
  partyStartDropZoneScroll();
  addTask(TaskKind::PartyDropZoneStart, inMode_ ? 0 : 130);
  if (!inMode_) addTask(TaskKind::PartyTunnelFreeze);
  silenceEffect_ = false;
}

void Table::partyTunnelTilt() {
  ballTeleport(Layer::Ground, {15, 47}, {0, 0});
  partyStartDropZone();
}

void Table::partyArcade() {
  modeCountRamp();
  if (tilted_ || !party_.arcadeOpen) {
    partyStartDropZone();
    return;
  }
  party_.arcadeOpen = false;
  lightSetAll(LightBind::PartyArcade, false);
  if (effect(EffectBind::PartyArcade)) {
    party_.arcadeReady = false;
    setMusicSilence();
    addTask(TaskKind::PartyArcadePickReward);
  } else {
    party_.arcadeReady = true;
    partyArcadePickReward();
  }
  partyStartDropZoneScroll();
  ballTeleport(Layer::Ground, {15, 47}, {0, 0});
}

void Table::partyArcadePickReward() {
  u16 delay;
  switch (rand(6)) {
    case 0:  // side extra ball
      lightSet(LightBind::PartySideExtraBall, 0, true);
      delay = effect(EffectBind::PartyArcadeSideExtraBall) ? 160 : 10;
      break;
    case 1:  // crazy letter
      if (partyCrazyLetter(EffectBind::PartyArcadeCrazy)) {
        addTask(TaskKind::PartyArcadeDropZoneStart, 140, 180, false);
        return;
      }
      delay = 10;
      break;
    case 2:
      if (effect(EffectBind::PartyArcade1M)) {
        addTask(TaskKind::PartyArcadeDropZoneStart, 120, 150, false);
        return;
      }
      delay = 10;
      break;
    case 3:
      if (effect(EffectBind::PartyArcade5M)) {
        addTask(TaskKind::PartyArcadeDropZoneStart, 110, 140, false);
        return;
      }
      delay = 10;
      break;
    case 4:
      if (effect(EffectBind::PartyArcade500k)) {
        addTask(TaskKind::PartyArcadeDropZoneStart, 45, 70, false);
        return;
      }
      delay = 10;
      break;
    default:
      effect(EffectBind::PartyArcadeNoScore);
      delay = 45;
      break;
  }
  addTask(TaskKind::PartyDropZoneStart, delay);
}

void Table::partyRampSnack() {
  PartyState& p = party_;
  if (p.inSnack) return;
  p.inSnack = true;
  scorePremult(Bcd::of("50000"), Bcd::of("5000"));
  modeCountRamp();
  if (p.snackLit[2]) {
    effect(EffectBind::PartySnack2);
    incrJackpot();
    if (p.popcorns < 2) ++p.popcorns;
  } else if (p.snackLit[1]) {
    effect(EffectBind::PartySnack1);
    incrJackpot();
  } else if (p.snackLit[0]) {
    effect(EffectBind::PartySnack0);
    incrJackpot();
  } else {
    effect(EffectBind::PartySnackNope);
  }
  lightSetAll(LightBind::PartySnack, false);
  if (p.snackLit[2]) {
    if (!lightState(LightBind::PartyParty, 1)) {
      lightSet(LightBind::PartyParty, 1, true);
      effect(EffectBind::PartyPartyA);
      partyCheckPartyAll();
    }
    if (p.popcorns == 1) {
      if (!p.orbitRightHb) {
        p.orbitRightHb = true;
        lightBlink(LightBind::PartyRightOrbitHoldBonus, 0, 12, static_cast<u8>((p.lightPhaseOrbitSpecial + 12) % 24));
      }
    } else if (!p.orbitRightDb) {
      p.orbitRightDb = true;
      lightBlink(LightBind::PartyRightOrbitDoubleBonus, 0, 12, p.lightPhaseOrbitSpecial);
      addTask(TaskKind::PartyDoubleBonusBlink);
    }
  }
  p.snackLit = {};
  ballTeleportFreeze(Layer::Overhead, {3, 253});
  addTask(TaskKind::PartySnacksRelease);
}

void Table::partyDemon() {
  PartyState& p = party_;
  if (p.inDemon) return;
  p.inDemon = true;
  ballTeleportFreeze(Layer::Ground, {257, 310});
  bool gotSomething = false;
  u16 timeout = 85;
  if (p.demon5m) {
    p.demon5m = false;
    effect(EffectBind::PartyDemon5M);
    lightSet(LightBind::PartyDemon5M, 0, false);
    gotSomething = true;
    timeout = inMode_ ? 15 : 160;
  }
  if (p.demonExtraBall) {
    p.demonExtraBall = false;
    effect(EffectBind::PartyDemonExtraBall);
    extraBall();
    lightSet(LightBind::PartyDemonExtraBall, 0, false);
    gotSomething = true;
    timeout = inMode_ ? 15 : 320;
  }
  if (p.demonJackpot || p.demonJackpotTimed) {
    p.demonJackpot = false;
    p.demonJackpotTimed = false;
    lightSet(LightBind::PartyDemonJackpot, 0, false);
    playJingleBind(JingleBind::PartyJackpot);
    if (!inMode_)
      startScript(ScriptBind::PartyJackpot);
    else if (inModeHit_)
      startScript(ScriptBind::PartyJackpotModeHit);
    else
      startScript(ScriptBind::PartyJackpotModeRamp);
    scoreMain_ += scoreJackpot_;
    scoreJackpot_ = assets_.scoreJackpotInit;
    gotSomething = true;
    timeout = 410;
  }
  if (!gotSomething) effect(EffectBind::PartyDemon250k);
  addTask(TaskKind::PartyDemonBlink, timeout);
}

void Table::partyLaneOuter() {
  if (lightState(LightBind::PartySideExtraBall, 0)) {
    lightSet(LightBind::PartySideExtraBall, 0, false);
    effect(EffectBind::PartySideExtraBall);
    extraBall();
    blockDrain_ = true;
    addTask(TaskKind::PartySideExtraBallFinish);
  } else {
    playSfxBind(SfxBind::RollTrigger);
    score(Bcd::of("50030"), Bcd::kZero);
  }
}

void Table::partySkyrideTop() {
  PartyState& p = party_;
  modeCountRamp();
  raisePhysmap(PhysmapBind::PartyGateSkyride);
  if (p.timeoutPartyT != 0) {
    p.timeoutPartyT = 0;
    partyParty(3);
  }
  p.timeoutPartyT = 600;
  switch (p.skyride) {
    case 0:
      effect(EffectBind::PartySkyride0);
      lightSet(LightBind::PartySkyride, 0, true);
      p.skyride = 1;
      break;
    case 1:
      effect(EffectBind::PartySkyride1);
      lightSet(LightBind::PartySkyride, 1, true);
      p.skyride = 2;
      break;
    default:
      effect(EffectBind::PartySkyride2);
      for (u8 i = 0; i < 3; ++i) lightBlink(LightBind::PartySkyride, i, 2, 0);
      p.skyride = 0;
      addTask(TaskKind::PartySkyrideUnblink);
      if (!p.orbitRightMb && !lightState(LightBind::PartyBonus, 3)) {
        p.orbitRightMb = true;
        lightBlink(LightBind::PartyRightOrbitMultiBonus, 0, 12, p.lightPhaseOrbitSpecial);
        effect(EffectBind::PartySkyrideLitMb);
      }
      break;
  }
}

void Table::partyPuke(u8 which) {
  PartyState& p = party_;
  playSfxBind(SfxBind::RollTrigger);
  if (lightState(LightBind::PartyPuke, which)) return;
  lightSet(LightBind::PartyPuke, which, true);
  scorePremult(Bcd::of("20070"), Bcd::of("1000"));
  incrJackpot();
  if (lightAllLit(LightBind::PartyPuke)) {
    const u8 other = static_cast<u8>((p.lightPhasePuke + 2) % 4);
    lightBlink(LightBind::PartyPuke, 0, 2, p.lightPhasePuke);
    lightBlink(LightBind::PartyPuke, 2, 2, p.lightPhasePuke);
    lightBlink(LightBind::PartyPuke, 1, 2, other);
    lightBlink(LightBind::PartyPuke, 3, 2, other);
    addTask(TaskKind::PartyPukeUnblinkAll);
    switch (p.demonReward) {
      case 0:
        p.demon5m = true;
        lightBlink(LightBind::PartyDemon5M, 0, 14, p.lightPhaseDemon);
        p.demonReward = 1;
        break;
      case 1:
        p.demonExtraBall = true;
        lightBlink(LightBind::PartyDemonExtraBall, 0, 14, static_cast<u8>((p.lightPhaseDemon + 14) % 28));
        p.demonReward = 2;
        break;
      case 2:
        p.demonJackpot = true;
        lightBlink(LightBind::PartyDemonJackpot, 0, 14, p.lightPhaseDemon);
        p.demonReward = 3;
        break;
      default: break;
    }
    partyParty(4);
  } else {
    p.flipperLockPuke = true;
    lightBlink(LightBind::PartyPuke, which, 2, 0);
    addTask(TaskKind::PartyPukeUnblink, which);
  }
}

void Table::partyRampCyclone() {
  PartyState& p = party_;
  modeCountRamp();
  if (p.timeoutSkillShot != 0) {
    incrJackpot();
    incrJackpot();
    p.scoreCycloneSkillShot += Bcd::of("1000000");
    scoreMain_ += p.scoreCycloneSkillShot;
    effect(EffectBind::PartyCycloneSkillShot);
    silenceEffect_ = true;
    partyParty(2);
  } else if (p.timeoutPartyPr != 0) {
    partyParty(2);
  }
  if (p.cycloneX5) {
    addCyclone(5);
    effect(EffectBind::PartyCycloneX5);
    lightSet(LightBind::PartyCycloneX5, 0, false);
    p.cycloneX5 = false;
  } else {
    addCyclone(1);
    effect(EffectBind::PartyCyclone);
  }
  silenceEffect_ = false;
}

bool Table::runPartyTask(Task& t) {
  PartyState& p = party_;
  const u8 which = static_cast<u8>(t.a);
  switch (t.kind) {
    case TaskKind::PartyDropZoneStart: partyStartDropZone(); break;
    case TaskKind::PartyDropZoneWait:
      lightBlink(LightBind::PartyDrop, 0, 7, 0);
      lightBlink(LightBind::PartyDrop, 1, 7, 0);
      addTask(TaskKind::PartyDropZoneRelease);
      break;
    case TaskKind::PartyDropZoneRelease:
      scroll_.targetSpecial.reset();
      ballTeleport(Layer::Overhead, {15, 47}, {0, static_cast<i16>(rand(0x80))});
      playSfxBind(SfxBind::IssueBall);
      lightSetAll(LightBind::PartyDrop, false);
      break;
    case TaskKind::PartyDropZoneScroll:
      if (t.a >= 5) {
        t.a = static_cast<u16>(t.a - 5);
        scroll_.setSpecialTargetNow(t.a);
        return true;
      }
      scroll_.setSpecialTargetNow(0);
      break;
    case TaskKind::PartyOrbitRightUnblink:
      lightSetAll(LightBind::PartyRightOrbitScore, false);
      lightBlink(LightBind::PartyRightOrbitScore, 0, 9, 0);
      p.orbitRightBlinking = false;
      break;
    case TaskKind::PartyMadUnblink: lightSet(LightBind::PartyMad, which, true); break;
    case TaskKind::PartyMadAllUnblink:
      lightSetAll(LightBind::PartyMad, false);
      p.madBlinking = false;
      break;
    case TaskKind::PartySecretDrop:
      if (!p.secretDropRelease) return true;
      addTask(TaskKind::PartyCycloneX5Blink);
      partyStartDropZone();
      break;
    case TaskKind::PartyCycloneX5Blink:
      if (p.cycloneX5) {
        lightBlink(LightBind::PartyCycloneX5, 0, 2, 0);
        addTask(TaskKind::PartyCycloneX5End);
      } else {
        lightSet(LightBind::PartyCycloneX5, 0, false);
      }
      break;
    case TaskKind::PartyCycloneX5End:
      p.cycloneX5 = false;
      lightSet(LightBind::PartyCycloneX5, 0, false);
      break;
    case TaskKind::PartyTunnelFreeze: ballTeleport(Layer::Ground, {15, 47}, {0, 0}); break;
    case TaskKind::PartyArcadePickReward:
      if (!(inMode_ || p.arcadeReady)) return true;
      if (tilted_) {
        partyStartDropZone();
      } else {
        setMusicMain();
        sequencer_->forceEndLoop();
        partyArcadePickReward();
      }
      break;
    case TaskKind::PartyArcadeDropZoneStart:
      // a: soft timeout, b: hard timeout, flag: jingle seen.
      --t.b;
      if (inMode_ || t.b == 0) {
        partyStartDropZone();
      } else {
        if (t.a != 0) --t.a;
        if (sequencer_->jinglePlaying() || t.flag) {
          t.flag = true;
          if (t.a == 0)
            partyStartDropZone();
          else
            return true;
        } else {
          return true;
        }
      }
      break;
    case TaskKind::PartyDoubleBonusBlink:
      if (p.orbitRightDb) {
        lightBlink(LightBind::PartyRightOrbitDoubleBonus, 0, 2, 0);
        addTask(TaskKind::PartyDoubleBonusEnd);
      }
      break;
    case TaskKind::PartyDoubleBonusEnd:
      if (p.orbitRightDb) {
        lightSet(LightBind::PartyRightOrbitDoubleBonus, 0, false);
        p.orbitRightDb = false;
      }
      break;
    case TaskKind::PartySnacksRelease:
      playSfxBind(SfxBind::PartySnacksRelease);
      ballTeleport(Layer::Overhead, {3, 253}, {0, -2500});
      addTask(TaskKind::PartySnacksFinish);
      break;
    case TaskKind::PartySnacksFinish: p.inSnack = false; break;
    case TaskKind::PartyDemonBlink:
      lightBlink(LightBind::PartyDemonHead, 0, 7, 0);
      addTask(TaskKind::PartyDemonRelease);
      break;
    case TaskKind::PartyDemonRelease:
      lightSet(LightBind::PartyDemonHead, 0, false);
      playSfxBind(SfxBind::IssueBall);
      ballTeleport(Layer::Ground, {257, 310}, {-575, 1575});
      p.inDemon = false;
      break;
    case TaskKind::PartySideExtraBallFinish: blockDrain_ = false; break;
    case TaskKind::PartySkyrideUnblink: lightSetAll(LightBind::PartySkyride, false); break;
    case TaskKind::PartyPukeUnblink:
      lightSet(LightBind::PartyPuke, which, true);
      p.flipperLockPuke = false;
      break;
    case TaskKind::PartyPukeUnblinkAll: lightSetAll(LightBind::PartyPuke, false); break;
    case TaskKind::PartyResetArcadeButton: p.arcadeButtonJustHit = false; break;
    case TaskKind::PartyDuckDrop: {
      static constexpr PhysmapBind kDuck[3] = {PhysmapBind::PartyHitDuck0, PhysmapBind::PartyHitDuck1,
                                               PhysmapBind::PartyHitDuck2};
      dropPhysmap(kDuck[which]);
      break;
    }
    case TaskKind::PartyDuckUnblink: lightSet(LightBind::PartyDuck, which, true); break;
    case TaskKind::PartyDuckAllUnblink:
      lightSetAll(LightBind::PartyDuck, false);
      lightSetAll(LightBind::PartyDuckDrop, true);
      raisePhysmap(PhysmapBind::PartyHitDuck0);
      raisePhysmap(PhysmapBind::PartyHitDuck1);
      raisePhysmap(PhysmapBind::PartyHitDuck2);
      playSfxBind(SfxBind::RaiseHitTargets);
      p.duckHit = {};
      break;
    case TaskKind::PartyHappyHour: partyHappyHour(); break;
    case TaskKind::PartyMegaLaugh: partyMegaLaugh(); break;
    default: break;
  }
  return false;
}

}  // namespace pfr
