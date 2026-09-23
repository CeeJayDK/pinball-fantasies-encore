// Billion Dollar Gameshow rules (translated from pfr's src/table/show.rs and its tasks).
#include "table/Table.h"

#include <utility>

namespace pfr {

void Table::showFrame() {
  ShowState& s = show_;
  auto countdown = [&](u16& t, LightBind light, u8 blinkHalf) {
    if (t == 0) return;
    if (--t == 0)
      lightSet(light, 0, false);
    else if (t == 120)
      lightBlink(light, 0, blinkHalf, 0);
  };
  countdown(s.timeoutSuperJackpot, LightBind::ShowSuperJackpot, 2);
  countdown(s.timeoutJackpot, LightBind::ShowJackpot, 2);
  if (++s.lightPhasePrize == 20) s.lightPhasePrize = 0;
  if (s.timeoutWheelTick != 0 && --s.timeoutWheelTick == 0) showWheelTick();
  for (u16* t : {&s.timeoutMb, &s.timeoutTv, &s.timeoutTrip, &s.timeoutCar, &s.timeoutBoat, &s.timeoutHouse, &s.timeoutPlane})
    if (*t != 0) --*t;
  countdown(s.timeoutCashpotX5, LightBind::ShowCashpotX5, 2);
  countdown(s.timeoutTopLoop, LightBind::ShowTopLoop, 3);
}

void Table::showFlipperPressed() {}

void Table::showDrained() {
  sequencer_->resetPriority();
  effect(EffectBind::Drained);
  setMusicSilence();
  addTask(TaskKind::DrainSfx);
}

void Table::showModeCheck() {
  if (modeTimeoutSecs_ != 0) return;
  playJingleBind(JingleBind::ModeEndHit);
  sequencer_->setMusic(3);
  sequencer_->resetPriority();
  inModeHit_ = false;
  inModeRamp_ = false;
}

void Table::showHitCenter(u8 which) {
  modeCountHit();
  if (!lightState(LightBind::ShowDropCenter, which)) return;
  effect(EffectBind::ShowDropCenter);
  playSfxBind(SfxBind::ShowHitTrigger);
  dropPhysmap(which == 0 ? PhysmapBind::ShowHitCenter0 : PhysmapBind::ShowHitCenter1);
  lightSet(LightBind::ShowDropCenter, which, false);
  if (lightAllUnlit(LightBind::ShowDropCenter)) addTask(TaskKind::ShowResetDropCenter);
}

void Table::showHitLeft(u8 which) {
  modeCountHit();
  if (!lightState(LightBind::ShowDropLeft, which)) return;
  effect(EffectBind::ShowDropLeft);
  playSfxBind(SfxBind::ShowHitTrigger);
  dropPhysmap(which == 0 ? PhysmapBind::ShowHitLeft0 : PhysmapBind::ShowHitLeft1);
  lightSet(LightBind::ShowDropLeft, which, false);
  if (lightAllUnlit(LightBind::ShowDropLeft)) addTask(TaskKind::ShowResetDropLeft);
}

void Table::showHitDollar(u8 which) {
  modeCountHit();
  playSfxBind(SfxBind::ShowHitTrigger);
  lightSet(LightBind::ShowDollar, which, true);
  if (lightAllLit(LightBind::ShowDollar)) {
    for (u8 i = 0; i < 2; ++i) lightBlink(LightBind::ShowDollar, i, 2, 0);
    effect(EffectBind::ShowDollarBoth);
    addTask(TaskKind::ShowUnblinkDollarAll);
    lightSet(LightBind::ShowSpinWheel, 0, true);
    lightBlink(LightBind::ShowSpinWheel, 0, 10, static_cast<u8>((show_.lightPhasePrize + 10) % 20));
    dropPhysmap(PhysmapBind::ShowGateVaultEntry);
  } else {
    lightBlink(LightBind::ShowDollar, which, 6, 0);
    effect(EffectBind::ShowDollar);
    addTask(TaskKind::ShowUnblinkDollar, which);
  }
}

void Table::showVault() {
  ShowState& s = show_;
  dropPhysmap(PhysmapBind::ShowGateVaultExit);
  ballTeleportFreeze(Layer::Ground, {4, 529});
  if (inMode_ || tilted_) {
    addTask(TaskKind::ShowVaultEject);
  } else if (s.billionLit) {
    s.billionLit = false;
    effect(EffectBind::ShowBillion);
    lightBlink(LightBind::ShowBillion, 0, 4, 0);
    addTask(TaskKind::ShowBillionRelease);
  } else {
    const bool collect = lightState(LightBind::ShowCollectPrize, 0);
    if (!collect) raisePhysmap(PhysmapBind::ShowGateVaultEntry);
    playJingleBind(JingleBind::ShowSpinWheel);
    s.wheelCycle = 0;
    s.timeoutWheelTick = s.wheelTiming[0];
    startScript(ScriptBind::ShowSpinWheelClearHalt);
    lightSetAll(LightBind::ShowWheel, false);
    u8 target = 0;
    if (!collect) {
      target = static_cast<u8>(rand(8));
    } else {
      // The wheel stops on the lit prize, in this order.
      static constexpr std::pair<std::size_t, u8> kOrder[6] = {{0, 0}, {1, 1}, {2, 2}, {3, 6}, {4, 5}, {5, 4}};
      for (const auto& [prize, pos] : kOrder)
        if (s.prizes[prize] == PrizeState::Lit) {
          target = pos;
          break;
        }
    }
    s.wheelPos = static_cast<u8>((target - static_cast<u8>(s.wheelTiming.size())) & 7);
    scroll_.targetSpecial = options_.resolution == Resolution::Normal ? 270
                            : options_.resolution == Resolution::High ? 220
                                                                      : 0;
  }
}

void Table::showWheelTick() {
  ShowState& s = show_;
  if (++s.wheelCycle == s.wheelTiming.size()) {
    startScript(ScriptBind::ShowSpinWheelBlink);
    addTask(lightState(LightBind::ShowCollectPrize, 0) ? TaskKind::ShowGivePrize : TaskKind::ShowSpinWheelEnd);
  } else {
    s.timeoutWheelTick = s.wheelTiming[s.wheelCycle];
    lightSetAll(LightBind::ShowWheel, false);
    s.wheelPos = static_cast<u8>((s.wheelPos + 1) % 8);
    lightSet(LightBind::ShowWheel, s.wheelPos, true);
    startScript(ScriptBind::ShowSpinWheelScore);
  }
}

void Table::showGivePrize() {
  ShowState& s = show_;
  static constexpr EffectBind kPrize[6] = {EffectBind::ShowPrizeTv,   EffectBind::ShowPrizeTrip,
                                           EffectBind::ShowPrizeCar,  EffectBind::ShowPrizeBoat,
                                           EffectBind::ShowPrizeHouse, EffectBind::ShowPrizePlane};
  for (u8 i = 0; i < 6; ++i) {
    if (s.prizes[i] == PrizeState::Taken) continue;
    s.prizes[i] = PrizeState::Taken;
    lightSet(LightBind::ShowPrize, i, true);
    sequencer_->resetPriority();
    effect(kPrize[i]);
    if (i == 2) {
      lightSet(LightBind::ShowCollectPrize, 0, false);
      raisePhysmap(PhysmapBind::ShowGateVaultEntry);
      lightSet(LightBind::ShowJackpot, 0, true);
      lightBlink(LightBind::ShowJackpot, 0, 10, s.lightPhasePrize);
      s.timeoutJackpot = 1500;
      s.prizeSets = 1;
    } else if (i == 5) {
      lightSet(LightBind::ShowCollectPrize, 0, false);
      raisePhysmap(PhysmapBind::ShowGateVaultEntry);
      s.prizeSets = 2;
    }
    return;
  }
}

Bcd Table::showWheelScore() const {
  static const Bcd kScores[8] = {Bcd::of("25000"),  Bcd::of("50000"),   Bcd::of("100000"),  Bcd::of("250000"),
                                 Bcd::of("500000"), Bcd::of("1000000"), Bcd::of("2500000"), Bcd::of("5000000")};
  return kScores[show_.wheelPos & 7];
}

void Table::showCashpot() {
  ShowState& s = show_;
  if (inMode_) {
    ballTeleportFreeze(Layer::Ground, {103, 233});
    lightSet(LightBind::ShowCashpot, 0, true);
    addTask(TaskKind::ShowCashpotEject, 30);
  } else if (s.prizeSets == 2) {
    lightSet(LightBind::ShowCashpot, 0, true);
    lightBlink(LightBind::ShowBillion, 0, 10, s.lightPhasePrize);
    s.billionLit = true;
    effect(EffectBind::ShowCashpotLock);
    sequencer_->setMusic(0);
    sequencer_->resetPriority();
    ballTeleport(Layer::Ground, {304, 535}, {10, 0});
    dropPhysmap(PhysmapBind::ShowGateVaultEntry);
  } else {
    incrJackpot();
    if (s.timeoutCashpotX5 != 0) {
      s.timeoutCashpotX5 = 10;
      Effect e = *assets_.effect(EffectBind::ShowCashpotX5);
      for (int i = 0; i < 5; ++i) e.scoreMain += s.scoreCashpot;
      effectRaw(e);
    } else {
      Effect e = *assets_.effect(EffectBind::ShowCashpot);
      e.scoreMain = s.scoreCashpot;
      effectRaw(e);
    }
    ballTeleportFreeze(Layer::Ground, {103, 233});
    addTask(TaskKind::ShowCashpot);
  }
}

void Table::showCashpotEject() {
  playSfxBind(SfxBind::ShowEjectCashpot);
  lightSet(LightBind::ShowCashpot, 0, false);
  ballTeleport(Layer::Ground, {103, 233}, {83, 1416});
}

void Table::showRampRight() {
  ShowState& s = show_;
  s.scoreCashpot += Bcd::of("7130");
  modeCountRamp();
  effect(EffectBind::ShowRampRight);
  s.timeoutTv = 240;
  s.timeoutCar = 240;
  dropPhysmap(PhysmapBind::ShowGateRampRight);
  if (s.timeoutJackpot != 0) {
    s.timeoutJackpot = 1;
    Effect e = *assets_.effect(EffectBind::ShowJackpot);
    e.scoreMain = scoreJackpot_;
    effectRaw(e);
    scoreJackpot_ = assets_.scoreJackpotInit;
    s.timeoutSuperJackpot = 300;
    lightBlink(LightBind::ShowSuperJackpot, 0, 10, 0);
    playJingleBind(JingleBind::ShowJackpot);
  }
}

void Table::showLitPrize(u8 which) {
  ShowState& s = show_;
  s.prizes[which] = PrizeState::Lit;
  static constexpr EffectBind kLit[6] = {EffectBind::ShowLitTv,   EffectBind::ShowLitTrip,  EffectBind::ShowLitCar,
                                         EffectBind::ShowLitBoat, EffectBind::ShowLitHouse, EffectBind::ShowLitPlane};
  effect(kLit[which]);
  const u8 other = static_cast<u8>((s.lightPhasePrize + 10) % 20);
  lightBlink(LightBind::ShowPrize, which, 10, (which == 1 || which == 4) ? other : s.lightPhasePrize);
  const auto lit = [&](std::size_t i) { return s.prizes[i] == PrizeState::Lit; };
  if ((lit(0) && lit(1) && lit(2)) || (lit(3) && lit(4) && lit(5))) {
    lightSet(LightBind::ShowCollectPrize, 0, true);
    lightBlink(LightBind::ShowCollectPrize, 0, 10, other);
    dropPhysmap(PhysmapBind::ShowGateVaultEntry);
  }
}

void Table::showRampLoop() {
  ShowState& s = show_;
  s.scoreCashpot += Bcd::of("7130");
  incrJackpot();
  modeCountRamp();
  effect(EffectBind::ShowRampLoop);
  if (s.timeoutSuperJackpot != 0) {
    s.timeoutSuperJackpot = 1;
    effect(EffectBind::ShowSuperJackpot);
  }
  s.timeoutCashpotX5 = 660;
  lightBlink(LightBind::ShowCashpotX5, 0, 10, static_cast<u8>((s.lightPhasePrize + 10) % 20));
  if (s.timeoutCar != 0) {
    s.timeoutCar = 0;
    if (s.prizes[2] == PrizeState::None) {
      showLitPrize(2);
    } else if (s.prizes[5] == PrizeState::None && s.prizeSets == 1) {
      s.timeoutPlane = 600;
      if (!inMode_) startScript(ScriptBind::ShowHintLoopLeft);
    } else {
      scoreRaisingMillions_ += Bcd::of("1000000");
      Effect e = *assets_.effect(EffectBind::ShowRaisingMillions);
      e.scoreMain = scoreRaisingMillions_;
      effectRaw(e);
    }
  }
  if (s.timeoutMb != 0) {
    const u8 which = lightSequence(LightBind::ShowBonus);
    if (which < 6) {
      static constexpr ScriptBind kMb[6] = {ScriptBind::ShowMbX2, ScriptBind::ShowMbX3, ScriptBind::ShowMbX4,
                                            ScriptBind::ShowMbX6, ScriptBind::ShowMbX8, ScriptBind::ShowMbX10};
      static constexpr u8 kMult[6] = {2, 3, 4, 6, 8, 10};
      if (playJingleBind(JingleBind::ShowMultiBonus)) startScript(kMb[which]);
      bonusMultLate_ = kMult[which];
    }
  }
}

void Table::showOrbitLeft() {
  effect(EffectBind::ShowOrbitLeft);
  if (!prevRollTrigger_ || prevRollTrigger_->kind != RollTrigger::ShowOrbitRight) return;
  ShowState& s = show_;
  if (s.timeoutBoat != 0 && s.prizes[3] == PrizeState::None) {
    s.timeoutBoat = 0;
    showLitPrize(3);
  }
  if (s.timeoutHouse != 0 && s.prizes[4] == PrizeState::None) {
    s.timeoutHouse = 0;
    showLitPrize(4);
  }
}

void Table::showOrbitRight() {
  modeCountRamp();
  effect(EffectBind::ShowOrbitRight);
  if (!prevRollTrigger_ || prevRollTrigger_->kind != RollTrigger::ShowOrbitLeft) return;
  ShowState& s = show_;
  if (lightState(LightBind::ShowOrbitExtraBall, 0)) {
    lightSet(LightBind::ShowOrbitExtraBall, 0, false);
    effect(EffectBind::ShowExtraBall);
    extraBall();
  }
  if (s.timeoutPlane != 0 && s.prizes[5] == PrizeState::None) {
    s.timeoutPlane = 0;
    showLitPrize(5);
  } else {
    raisePhysmap(PhysmapBind::ShowGateRampRight);
    s.timeoutMb = 240;
    s.timeoutTrip = 240;
  }
}

void Table::showRampSkills() {
  ShowState& s = show_;
  effect(EffectBind::ShowRampSkills);
  incrJackpot();
  s.scoreCashpot += Bcd::of("7130");
  modeCountRamp();
  addCyclone(1);
  numCycloneTarget_ = static_cast<u16>(numCyclone_ / 6 * 6 + 6);
  const int n = numCyclone_, m = numCyclone_ % 12;
  auto moneyMania = [&](bool hit) {
    effect(hit ? EffectBind::ShowModeHit : EffectBind::ShowModeRamp);
    inMode_ = true;
    (hit ? inModeHit_ : inModeRamp_) = true;
    lightSet(LightBind::ShowMoneyMania, 0, true);
  };
  if (n <= 5) {
    effect(EffectBind::ShowSkillsToMoneyMania);
  } else if (n == 6) {
    moneyMania(true);
  } else if (n <= 11) {
    effect(EffectBind::ShowSkillsToExtraBall);
  } else if (n == 12) {
    playJingleBind(JingleBind::ShowExtraBallLit);
    lightSet(LightBind::ShowOrbitExtraBall, 0, true);
    lightBlink(LightBind::ShowOrbitExtraBall, 0, 15, static_cast<u8>((s.lightPhasePrize + 10) % 20));
  } else if (m == 6) {
    moneyMania(false);
  } else if (m == 0) {
    moneyMania(true);
  } else {
    effect(EffectBind::ShowSkillsToMoneyMania);
  }
  if (s.timeoutTv != 0) {
    if (s.prizes[0] == PrizeState::None) {
      s.timeoutTv = 0;
      showLitPrize(0);
    } else if (s.prizes[3] == PrizeState::None && s.prizeSets == 1) {
      s.timeoutBoat = 600;
      if (!inMode_) startScript(ScriptBind::ShowHintLoopRight);
    }
  }
  if (s.timeoutTrip != 0) {
    if (s.prizes[1] == PrizeState::None) {
      s.timeoutTrip = 0;
      showLitPrize(1);
    } else if (s.prizes[4] == PrizeState::None && s.prizeSets == 1) {
      s.timeoutHouse = 600;
      if (!inMode_) startScript(ScriptBind::ShowHintLoopRight);
    }
  }
}

void Table::showRampTop() {
  ShowState& s = show_;
  s.scoreCashpot += Bcd::of("7130");
  effect(EffectBind::ShowRampTop);
  if (s.timeoutTopLoop != 0) effect(EffectBind::ShowRampTopTwice);
  s.timeoutTopLoop = 600;
  if (lightState(LightBind::ShowCollectPrize, 0)) playJingleBind(JingleBind::ShowPrizeIncoming);
  lightBlink(LightBind::ShowTopLoop, 0, 10, s.lightPhasePrize);
}

bool Table::runShowTask(Task& t) {
  const u8 which = static_cast<u8>(t.a);
  switch (t.kind) {
    case TaskKind::ShowResetDropCenter:
      playSfxBind(SfxBind::RaiseHitTargets);
      lightSetAll(LightBind::ShowDropCenter, true);
      raisePhysmap(PhysmapBind::ShowHitCenter0);
      raisePhysmap(PhysmapBind::ShowHitCenter1);
      break;
    case TaskKind::ShowResetDropLeft:
      playSfxBind(SfxBind::RaiseHitTargets);
      lightSetAll(LightBind::ShowDropLeft, true);
      raisePhysmap(PhysmapBind::ShowHitLeft0);
      raisePhysmap(PhysmapBind::ShowHitLeft1);
      break;
    case TaskKind::ShowUnblinkDollar: lightSet(LightBind::ShowDollar, which, true); break;
    case TaskKind::ShowUnblinkDollarAll: lightSetAll(LightBind::ShowDollar, false); break;
    case TaskKind::ShowVaultEject:
      playSfxBind(SfxBind::IssueBall);
      ball_.frozen = false;
      ball_.speed[1] = -3500;
      break;
    case TaskKind::ShowBillionRelease:
      lightSet(LightBind::ShowBillion, 0, false);
      lightSetAll(LightBind::ShowPrize, false);
      show_.prizes = {};
      show_.prizeSets = 0;
      playSfxBind(SfxBind::IssueBall);
      ball_.frozen = false;
      ball_.speed[1] = -3500;
      break;
    case TaskKind::ShowSpinWheelEnd:
      scroll_.targetSpecial.reset();
      scoreMain_ += showWheelScore();
      startScript(ScriptBind::ShowSpinWheelClear);
      ball_.frozen = false;
      ball_.speed[1] = -2916;
      lightSet(LightBind::ShowSpinWheel, 0, false);
      break;
    case TaskKind::ShowGivePrize: showGivePrize(); break;
    case TaskKind::ShowCashpot:
      addTask(TaskKind::ShowCashpotEject, 40);
      lightSet(LightBind::ShowCashpot, 0, true);
      break;
    case TaskKind::ShowCashpotEject: showCashpotEject(); break;
    default: break;
  }
  return false;
}

}  // namespace pfr
