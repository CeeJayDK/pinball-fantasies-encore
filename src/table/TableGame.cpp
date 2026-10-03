// Game flow, scoring, effects, per-player state and trigger dispatch (translated from pfr's
// src/table/{game,player,triggers}.rs).
#include "table/Table.h"

#include <array>
#include <optional>

namespace pfr {

// ---- game flow ------------------------------------------------------------------------------

void Table::initGame() {
  kbdState_ = KbdState::Main;
  enterAttract_ = false;
  curBall_ = 1;
  curPlayer_ = 1;
  gotTopScore_ = false;
  gotHighScore_ = false;
  inGameStart_ = true;
  scoreJackpot_ = assets_.scoreJackpotInit;
  resetPlayerState();
  extraBalls_ = 0;
  partyOn_ = false;
  matchDigit_.reset();
  scoreMain_ = Bcd::kZero;
  scoreBonus_ = Bcd::kZero;
  numCyclone_ = 0;
  bcdNumCyclone_ = Bcd::kZero;
  scoreCycloneBonus_ = Bcd::kZero;
}

void Table::resetPlayerState() {
  inMode_ = inModeHit_ = inModeRamp_ = false;
  scoreModeHit_ = Bcd::kZero;
  scoreModeRamp_ = Bcd::kZero;
  bonusMultEarly_ = 1;
  bonusMultLate_ = 1;
  holdBonus_ = false;
  lightsReset();
  switch (assets_.table) {
    case 0:
      party_ = PartyState{};
      lightSetAll(LightBind::PartyDuckDrop, true);
      lightBlink(LightBind::PartyTunnel, 0, 8, 0);
      lightBlink(LightBind::PartyRightOrbitScore, 0, 9, 0);
      raisePhysmap(PhysmapBind::PartyHitDuck0);
      raisePhysmap(PhysmapBind::PartyHitDuck1);
      raisePhysmap(PhysmapBind::PartyHitDuck2);
      break;
    case 1: speed_ = SpeedState{}; break;
    case 2:
      show_ = ShowState(hifps_);
      lightSetAll(LightBind::ShowDropCenter, true);
      lightSetAll(LightBind::ShowDropLeft, true);
      lightBlink(LightBind::ShowSkills, 0, 15, 0);
      raisePhysmap(PhysmapBind::ShowHitCenter0);
      raisePhysmap(PhysmapBind::ShowHitCenter1);
      raisePhysmap(PhysmapBind::ShowHitLeft0);
      raisePhysmap(PhysmapBind::ShowHitLeft1);
      raisePhysmap(PhysmapBind::ShowGateRampRight);
      raisePhysmap(PhysmapBind::ShowGateVaultEntry);
      raisePhysmap(PhysmapBind::ShowGateVaultExit);
      break;
    default: {
      stones_ = StonesState{};
      raisePhysmap(PhysmapBind::StonesGateTowerEntry);
      raisePhysmap(PhysmapBind::StonesGateKickback);
      const u8 target = static_cast<u8>(rand(3));
      stones_.keySkillshot = target;
      lightBlink(LightBind::StonesKey, target, 1, 0);
      break;
    }
  }
}

void Table::initBall() {
  rollTrigger_.reset();
  atSpring_ = true;
  flipperPressed_ = false;
  spacePressed_ = false;
  silenceEffect_ = false;
  inDrain_ = false;
  inMode_ = inModeHit_ = inModeRamp_ = false;
  timerStop_ = false;
  lightsReset();
  tasks_.clear();
  if (!specialPlungerEvent_) {
    dm_.stopBlink();
    if (inGameStart_)
      inGameStart_ = false;
    else
      startScript(ScriptBind::Main);
  }
  resetPlayerState();
  loadCurPlayer();
  if (assets_.table == 0) {
    lightSetAll(LightBind::PartyDuckDrop, true);
    raisePhysmap(PhysmapBind::PartyHitDuck0);
    raisePhysmap(PhysmapBind::PartyHitDuck1);
    raisePhysmap(PhysmapBind::PartyHitDuck2);
    if (extraBalls_ != 0) lightSet(LightBind::PartyExtraBall, 0, true);
  }
}

void Table::issueBall() {
  inDrain_ = false;
  drained_ = false;
  inPlunger_ = true;
  ballTeleportFreeze(Layer::Ground, assets_.issueBallPos);
  if (!inGameStart_ && !partyOn_)
    playJinglePlunger();
  else
    setMusicPlunger();
  initBall();
  ballScoredPoints_ = false;
  if (inGameStart_)
    addTask(TaskKind::IssueBallFinish);
  else
    issueBallFinish();
}

void Table::issueBallFinish() {
  addTask(TaskKind::IssueBallSfx);
  addTask(TaskKind::IssueBallRelease);
  if (assets_.sfx(SfxBind::RaiseHitTargets)) addTask(TaskKind::IssueBallRaiseSfx);
  flippersEnabled_ = true;
  tilted_ = false;
  tiltCounter_ = 0;
}

void Table::issueBallRelease() { ballTeleport(Layer::Ground, assets_.issueBallReleasePos, {10, 0}); }

void Table::abortGame() {
  abandoned_ = true;
  ballTeleport(Layer::Ground, {300, 570}, {0, 0});
  kbdState_ = KbdState::Main;
  addTask(TaskKind::GameOver);
  playJingleBindForce(JingleBind::Attract);
  dm_.stopBlink();
  startScript(ScriptBind::Attract);
}

void Table::score(const Bcd& main, const Bcd& bonus) {
  scoreMain_ += main;
  scoreBonus_ += bonus;
  ballScoredPoints_ = true;
  resetIdle();
}

void Table::scorePremult(const Bcd& main, const Bcd& bonus) {
  scoreMain_ += main;
  for (int i = 0; i < bonusMultEarly_; ++i) scoreBonus_ += bonus;
  ballScoredPoints_ = true;
  resetIdle();
}

void Table::effectForceRaw(const Effect& e) {
  if (e.jingle) sequencer_->playJingle(*e.jingle, true, std::nullopt);
  score(e.scoreMain, e.scoreBonus);
  if (e.script) startScriptRaw(*e.script);
}

bool Table::effectRaw(const Effect& e) {
  bool present;
  if (e.jingle) {
    if ((silenceEffect_ || inMode_) && e.jingle->position != jingle(JingleBind::Drained).position)
      present = false;
    else
      present = sequencer_->playJingle(*e.jingle, false, std::nullopt);
  } else {
    present = e.silentPriority >= sequencer_->priority();
  }
  score(e.scoreMain, e.scoreBonus);
  if (present && e.script) startScriptRaw(*e.script);
  return present;
}

void Table::effectForce(EffectBind bind) {
  if (const auto& e = assets_.effect(bind)) effectForceRaw(*e);
}

bool Table::effect(EffectBind bind) {
  const auto& e = assets_.effect(bind);
  return e ? effectRaw(*e) : false;
}

void Table::enter() {
  startKeysActive_ = false;
  inGameStart_ = false;
  const Jingle& j = jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Main);
  sequencer_->playJingle(j, true, j.position);
  startScript(ScriptBind::Main);
  inPlunger_ = false;
  atSpring_ = false;
  partyOn_ = false;
  specialPlungerEvent_ = false;
}

void Table::incrJackpot() { scoreJackpot_ += assets_.scoreJackpotIncr; }

void Table::extraBall() {
  ++extraBalls_;
  switch (assets_.table) {
    case 0: lightSet(LightBind::PartyExtraBall, 0, true); break;
    case 1: lightSet(LightBind::SpeedExtraBall, 0, true); break;
    case 2: lightSet(LightBind::ShowExtraBall, 0, true); break;
    default: break;
  }
}

void Table::addCyclone(u8 cnt) {
  numCyclone_ = static_cast<u16>(numCyclone_ + cnt);
  bcdNumCyclone_ += Bcd::fromDigit(cnt);
  Bcd b;
  b.digits[6] = cnt;
  scoreCycloneBonus_ += b;
  if (numCyclone_ == 1) addCyclone(1);
}

void Table::matchDone(u8 digit) {
  matchDigit_ = digit;
  bool any = false;
  for (const PlayerState& p : players_) any |= p.scoreMain.digits[10] == digit;
  if (!any) return;
  dm_.startBlink(3);
  for (std::size_t i = 0; i < players_.size(); ++i)
    if (players_[i].scoreMain.digits[10] != digit) dmPuts(DmFont::H5, {static_cast<i16>(i * 16), 0}, "_");
  playJingleBind(JingleBind::MatchWin);
  sequencer_->resetPriority();
}

// ---- per-player state (player.rs) --------------------------------------------------------------

template <std::size_t N>
std::array<bool, N> Table::lightSave(LightBind bind) const {
  std::array<bool, N> r{};
  for (std::size_t i = 0; i < N && i < assets_.lightsOf(bind).size(); ++i) r[i] = lightState(bind, static_cast<u8>(i));
  return r;
}

template <std::size_t N>
void Table::lightLoad(LightBind bind, const std::array<bool, N>& data) {
  for (std::size_t i = 0; i < N && i < assets_.lightsOf(bind).size(); ++i) lightSet(bind, static_cast<u8>(i), data[i]);
}

void Table::loadCurPlayer() {
  const PlayerState& p = players_[curPlayer_ - 1u];
  scoreMain_ = p.scoreMain;
  scoreBonus_ = p.scoreBonus;
  numCyclone_ = p.numCyclone;
  bcdNumCyclone_ = p.bcdNumCyclone;
  switch (assets_.table) {
    case 0:
      lightLoad(LightBind::PartyPuke, p.partyLightPuke);
      lightLoad(LightBind::PartyMad, p.partyLightMad);
      lightLoad(LightBind::PartyCrazy, p.partyLightCrazy);
      lightLoad(LightBind::PartyParty, p.partyLightParty);
      party_.scoreCycloneSkillShot = p.partyScoreCycloneSkillShot;
      party_.scoreTunnelSkillShot = p.partyScoreTunnelSkillShot;
      break;
    case 1:
      speed_.curGear = p.speedCurGear;
      speed_.curSpeed = p.speedCurSpeed;
      speed_.carMods = p.speedCarMods;
      speed_.curPlace = p.speedCurPlace;
      speed_.maxPlace = p.speedMaxPlace;
      lightSet(LightBind::SpeedPitStopGoal, 0, p.speedLightGoal);
      lightLoad(LightBind::SpeedCarPart, p.speedLightCar);
      lightLoad(LightBind::SpeedCarPartLit, p.speedLightCarLit);
      speedLoadFixup();
      break;
    case 2:
      show_.prizeSets = p.showPrizeSets;
      for (int i = 0; i < p.showPrizeSets * 3; ++i) {
        show_.prizes[static_cast<std::size_t>(i)] = PrizeState::Taken;
        lightSet(LightBind::ShowPrize, static_cast<u8>(i), true);
      }
      break;
    default:
      stones_.curGhost = p.stonesCurGhost;
      stones_.ghostActive = p.stonesGhostActive;
      stones_.scoreSkillShot = p.stonesScoreSkillShot;
      stones_.kickback = p.stonesKickback;
      lightLoad(LightBind::StonesRip, p.stonesLightRip);
      lightLoad(LightBind::StonesStone, p.stonesLightStone);
      lightLoad(LightBind::StonesBone, p.stonesLightBone);
      stonesLoadFixup();
      break;
  }
}

void Table::saveCurPlayer() {
  PlayerState p;
  p.scoreMain = scoreMain_;
  p.scoreBonus = scoreBonus_;
  p.numCyclone = numCyclone_;
  p.bcdNumCyclone = bcdNumCyclone_;
  switch (assets_.table) {
    case 0:
      p.partyLightPuke = lightSave<4>(LightBind::PartyPuke);
      p.partyLightMad = lightSave<3>(LightBind::PartyMad);
      p.partyLightCrazy = lightSave<5>(LightBind::PartyCrazy);
      p.partyLightParty = lightSave<5>(LightBind::PartyParty);
      p.partyScoreTunnelSkillShot = party_.scoreTunnelSkillShot;
      p.partyScoreCycloneSkillShot = party_.scoreCycloneSkillShot;
      break;
    case 1:
      p.speedCurGear = speed_.curGear;
      p.speedCurSpeed = speed_.curSpeed;
      p.speedCurPlace = speed_.curPlace;
      p.speedMaxPlace = speed_.maxPlace;
      p.speedCarMods = speed_.carMods;
      p.speedLightGoal = lightState(LightBind::SpeedPitStopGoal, 0);
      p.speedLightCarLit = lightSave<5>(LightBind::SpeedCarPartLit);
      p.speedLightCar = lightSave<5>(LightBind::SpeedCarPart);
      break;
    case 2: p.showPrizeSets = show_.prizeSets; break;
    default:
      p.stonesCurGhost = stones_.curGhost;
      p.stonesGhostActive = stones_.ghostActive;
      p.stonesScoreSkillShot = stones_.scoreSkillShot;
      p.stonesKickback = stones_.kickback;
      p.stonesLightRip = lightSave<3>(LightBind::StonesRip);
      p.stonesLightStone = lightSave<5>(LightBind::StonesStone);
      p.stonesLightBone = lightSave<4>(LightBind::StonesBone);
      break;
  }
  players_[curPlayer_ - 1u] = p;
}

// ---- triggers (triggers.rs) ----------------------------------------------------------------------

void Table::doHitTriggers() {
  if (tilted_) return;
  if (!hitPos_) return;
  auto hit = *hitPos_;
  hitPos_.reset();
  hit[1] = static_cast<i16>(hit[1] + push_.offset());
  if (ball_.layer != Layer::Ground) return;
  for (const HitTriggerArea& a : assets_.hitTriggers) {
    if (!a.rect.contains(hit[0], hit[1])) continue;
    if (trace) trace->push_back(std::string("hit ") + kHitTriggerNames[static_cast<std::size_t>(a.kind)] + " " + std::to_string(a.arg));
    switch (a.kind) {
      case HitTrigger::PartyArcadeButton: partyArcadeButton(); break;
      case HitTrigger::PartyDuck: partyHitDuck(a.arg); break;
      case HitTrigger::SpeedBur: speedHitBur(a.arg); break;
      case HitTrigger::SpeedNin: speedHitNin(a.arg); break;
      case HitTrigger::ShowDollar: showHitDollar(a.arg); break;
      case HitTrigger::ShowCenter: showHitCenter(a.arg); break;
      case HitTrigger::ShowLeft: showHitLeft(a.arg); break;
      case HitTrigger::StonesBone: stonesHitBone(a.arg); break;
      case HitTrigger::StonesStone: stonesHitStone(a.arg); break;
      default: break;
    }
    return;
  }
}

void Table::doRollTriggers() {
  const auto c = ballCenter();
  const auto& list = (tilted_ ? assets_.rollTriggersTilt : assets_.rollTriggers)[static_cast<std::size_t>(ball_.layer)];
  for (const RollTriggerArea& a : list) {
    if (!a.rect.contains(c[0], c[1])) continue;
    const RollTriggerId id{a.kind, a.arg};
    if (rollTrigger_ != id) {
      rollTrigger_ = id;
      doRollTrigger(id);
      prevRollTrigger_ = rollTrigger_;
    }
    return;
  }
  rollTrigger_.reset();
}

void Table::doRollTrigger(RollTriggerId t) {
  using R = RollTrigger;
  if (trace) trace->push_back(std::string("roll ") + kRollTriggerNames[static_cast<std::size_t>(t.kind)] + " " + std::to_string(t.arg));
  const auto prevIs = [&](R k) { return prevRollTrigger_ && prevRollTrigger_->kind == k; };
  switch (t.kind) {
    case R::Dummy: break;
    case R::PlungerBottom: atSpring_ = true; break;
    case R::PlungerGo:
      atSpring_ = false;
      if (assets_.table == 0) {
        party_.timeoutSkillShot = 300;
        party_.timeoutSpringLoop = 120;
      } else if (assets_.table == 2) {
        dropPhysmap(PhysmapBind::ShowGatePlunger);
      }
      break;
    case R::PartyLaneInner:
      effect(EffectBind::PartyRollInner);
      playSfxBind(SfxBind::RollInner);
      break;
    case R::PartyLaneOuter: partyLaneOuter(); break;
    case R::PartyOrbitTopLeft:
      if (prevIs(R::PartyOrbitTopRight)) {
        if (party_.timeoutSpringLoop != 0)
          party_.timeoutSpringLoop = 0;
        else
          partyOrbitRight();
      }
      break;
    case R::PartyOrbitTopRight:
      if (prevIs(R::PartyOrbitTopLeft)) partyOrbitLeft();
      break;
    case R::PartySecret: partySecret(); break;
    case R::PartyTunnel: partyTunnel(); break;
    case R::PartyArcade: partyArcade(); break;
    case R::PartyOrbitEntryRight: party_.timeoutSpringLoop = 0; break;
    case R::PartyEnter:
      if (prevIs(R::PlungerGo)) enter();
      break;
    case R::PartyDemon: partyDemon(); break;
    case R::PartySkyrideTop:
      if (prevIs(R::PartySkyrideRamp)) partySkyrideTop();
      break;
    case R::PartySkyrideRamp: dropPhysmap(PhysmapBind::PartyGateSkyride); break;
    case R::PartySkyridePuke: partyPuke(t.arg); break;
    case R::PartyRampCyclone: partyRampCyclone(); break;
    case R::PartyRampSnack: partyRampSnack(); break;
    case R::PartySecretTilt: partySecretTilt(); break;
    case R::PartyTunnelTilt: partyTunnelTilt(); break;
    case R::SpeedLaneInner:
      playSfxBind(SfxBind::RollInner);
      effect(EffectBind::SpeedLaneInner);
      break;
    case R::SpeedLaneOuter: effect(EffectBind::SpeedLaneOuter); break;
    case R::SpeedPitStop: speedPitStop(); break;
    case R::SpeedEnter:
      if (prevIs(R::SpeedPlungerExit)) enter();
      break;
    case R::SpeedPitLoopJump:
      if (prevIs(R::SpeedJumpPre))
        speedRampJump();
      else if (prevIs(R::SpeedPitLoopPre))
        speedPitLoop();
      break;
    case R::SpeedRampOffroad: speedRampOffroad(); break;
    case R::SpeedPitLoopPre: break;
    case R::SpeedPit: speedRollPit(t.arg); break;
    case R::SpeedOffroadExit: effect(EffectBind::SpeedOffroadExit); break;
    case R::SpeedRampMilesRight:
      speed_.timeoutMilesRight = 390;
      if (speed_.timeoutMilesLeft != 0) {
        speed_.timeoutMilesLeft = 0;
        speedOvertake();
      }
      speedBumpMiles();
      break;
    case R::SpeedRampMilesLeft:
      speed_.timeoutMilesLeft = 390;
      if (speed_.timeoutMilesRight != 0) {
        speed_.timeoutMilesRight = 0;
        speedOvertake();
      }
      speedBumpMiles();
      break;
    case R::SpeedJumpPre: break;
    case R::SpeedPlungerExit:
      ball_.speed[1] = 0;
      ball_.layer = Layer::Ground;
      break;
    case R::ShowLaneInner:
      effect(EffectBind::ShowLaneInner);
      playSfxBind(SfxBind::RollInner);
      break;
    case R::ShowLaneOuter:
      effect(EffectBind::ShowLaneOuter);
      playSfxBind(SfxBind::RollTrigger, 0x20);
      break;
    case R::ShowEnter:
      if (prevIs(R::PlungerGo)) {
        raisePhysmap(PhysmapBind::ShowGatePlunger);
        enter();
      }
      break;
    case R::ShowOrbitLeft: showOrbitLeft(); break;
    case R::ShowOrbitRight: showOrbitRight(); break;
    case R::ShowCashpot: showCashpot(); break;
    case R::ShowVault: showVault(); break;
    case R::ShowVaultExit: raisePhysmap(PhysmapBind::ShowGateVaultExit); break;
    case R::ShowRampSkillEntry: effect(EffectBind::ShowSkillsEntry); break;
    case R::ShowRampTopEntry: effect(EffectBind::ShowTopEntry); break;
    case R::ShowRampLoopEntry: effect(EffectBind::ShowLoopEntry); break;
    case R::ShowRampTop: showRampTop(); break;
    case R::ShowRampSkillMark: break;
    case R::ShowRampSkill:
      if (prevIs(R::ShowRampSkillMark)) showRampSkills();
      break;
    case R::ShowRampRight: showRampRight(); break;
    case R::ShowRampLoop: showRampLoop(); break;
    case R::ShowRampTopSecondary: incrJackpot(); break;
    case R::StonesLaneInnerLeft:
    case R::StonesLaneInnerRight:
      playSfxBind(SfxBind::RollInner);
      scorePremult(Bcd::of("10070"), Bcd::of("1080"));
      break;
    case R::StonesLaneOuterLeft:
      playSfxBind(SfxBind::RollTrigger);
      score(Bcd::of("500010"), Bcd::kZero);
      break;
    case R::StonesLaneOuterRight:
      playSfxBind(SfxBind::RollTrigger);
      score(Bcd::of("500030"), Bcd::kZero);
      break;
    case R::StonesKeyEntry: stonesRollKeyEntry(); break;
    case R::StonesRampTower:
      dropPhysmap(PhysmapBind::StonesGateRampTower);
      modeCountRamp();
      break;
    case R::StonesKey: stonesRollKey(t.arg); break;
    case R::StonesWell: stonesWell(); break;
    case R::StonesVault: stonesVault(); break;
    case R::StonesKeyClose: dropPhysmap(PhysmapBind::StonesGateRampTower); break;
    case R::StonesTower: stonesTower(); break;
    case R::StonesRampTop: stonesRampTop(); break;
    case R::StonesRip: stonesRollRip(t.arg); break;
    case R::StonesRampTopExit: stones_.timeoutTopLoop = 300; break;
    case R::StonesRampScreams: stonesRampScreams(); break;
    case R::StonesRampLeftToLane: stonesRampLeftToLane(); break;
    case R::StonesRampLeftToVault: stonesRampLeftToVault(); break;
    case R::StonesRampLeftFixup0: dropPhysmap(PhysmapBind::StonesGateRampLeft1); break;
    case R::StonesRampLeftFixup1: raisePhysmap(PhysmapBind::StonesGateRampLeft1); break;
    case R::StonesRampLeftFixup2: dropPhysmap(PhysmapBind::StonesGateRampLeft2); break;
    case R::StonesRampLeftFixup3: raisePhysmap(PhysmapBind::StonesGateRampLeft2); break;
    case R::StonesVaultExit:
      stones_.vaultFromRamp = true;
      stonesIncrVault();
      break;
    case R::StonesEnter:
      enter();
      ball_.layer = Layer::Ground;
      break;
    case R::StonesWellTilt: stonesWellTilt(); break;
    case R::StonesTowerTilt: stonesTowerTilt(); break;
    default: break;
  }
}

}  // namespace pfr
