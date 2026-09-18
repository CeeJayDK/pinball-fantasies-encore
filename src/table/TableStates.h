#pragma once
// Rule state for each table, and what is saved per player between balls
// (translated from pfr's src/table/{party,speed,show,stones,player}.rs).
#include <array>
#include <optional>
#include <vector>

#include "assets/Bcd.h"

namespace pfr {

struct PartyState {
  bool flipperLockPuke = false;
  u8 orbitRightCycle = 0;
  bool orbitRightBlinking = false, orbitRightMb = false, orbitRightHb = false, orbitRightDb = false;
  bool madBlinking = false, cycloneX5 = false, secretDropRelease = false;
  bool arcadeButtonJustHit = false, arcadeOpen = false, arcadeReady = false;
  std::array<bool, 3> duckHit{};
  u8 curSnack = 0;
  std::array<bool, 3> snackLit{};
  bool inSnack = false;
  u8 popcorns = 0;
  bool inDemon = false;
  u8 demonReward = 0;
  bool demon5m = false, demonExtraBall = false, demonJackpot = false, demonJackpotTimed = false;
  u8 skyride = 0;
  Bcd scoreCycloneSkillShot, scoreTunnelSkillShot;
  u8 lightPhaseSnack = 0, lightPhaseOrbitSpecial = 0, lightPhasePuke = 0, lightPhaseDemon = 0;
  u16 timeoutSkillShot = 0, timeoutPartyT = 0, timeoutPartyPr = 0, timeoutSpringLoop = 0, timeoutTunnel = 0;
};

struct SpeedState {
  std::array<bool, 3> blinkBur{}, blinkNin{};
  u16 timeoutPitAll = 0;
  std::array<u16, 3> timeoutPit{};
  u8 curPlace = 0, maxPlace = 0, curGear = 0, lightPhasePlace = 0;
  u16 timeoutGearBlink = 0, timeoutMilesLeft = 0, timeoutMilesRight = 0, timeoutJackpot = 0;
  u8 mbActive = 0, mbPending = 0, carMods = 0;
  bool pedalMetal = false;
  u8 curSpeed = 0;
};

enum class PrizeState : u8 { None, Lit, Taken };

struct ShowState {
  explicit ShowState(bool hifps = false);
  Bcd scoreCashpot = Bcd::of("500000");
  std::array<PrizeState, 6> prizes{};
  u8 prizeSets = 0;
  u16 timeoutWheelTick = 0, timeoutMb = 0, timeoutTopLoop = 0, timeoutTv = 0, timeoutTrip = 0, timeoutCar = 0,
      timeoutBoat = 0, timeoutHouse = 0, timeoutPlane = 0, timeoutCashpotX5 = 0, timeoutJackpot = 0,
      timeoutSuperJackpot = 0;
  bool billionLit = false;
  u8 lightPhasePrize = 0;
  std::size_t wheelCycle = 0;
  u8 wheelPos = 0;
  std::vector<u16> wheelTiming;
};

inline ShowState::ShowState(bool hifps) {
  if (hifps)
    wheelTiming = {4, 4, 4, 4, 4, 4, 4,  4,  4,  4,  4,  4,  4,  4,  4,  5,  5,  6,  7,  7,  7,
                   7, 8, 8, 8, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 14, 16, 19, 22, 25, 50};
  else
    wheelTiming = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,  4,  4,  5,  5,  5,  6,  6,
                   6, 6, 7, 7, 7, 8, 8, 8, 8, 9, 9, 10, 10, 12, 14, 17, 20, 24, 32, 47};
}

struct StonesState {
  bool flipperLockKey = false, flipperLockRip = false;
  u8 curGhost = 0;
  bool ghostActive = false, vaultFromRamp = false, inVault = false, vaultHold = false;
  std::array<bool, 4> boneBlinking{};
  std::array<bool, 5> stoneBlinking{};
  bool stonesBonesBlinking = false, ghostsBlinking = false, ballLocked = false, millionPlus = false;
  Bcd scoreMillionPlus, scoreSkillShot;
  bool keyBlinking = false;
  std::optional<u8> keySkillshot;
  u8 keyTowerCycle = 0;
  bool inTower = false, towerOpen = false, towerExtraBall = false, towerJackpot = false, towerSuperJackpot = false,
       tower1m = false, tower5m = false, towerDoubleBonus = false, towerHoldBonus = false, towerHunt = false;
  u8 towerHuntCtr = 0;
  bool towerResumeMode = false, towerResumeModeRamp = false;
  bool screamX2 = false, screamDemon = false, inWell = false, wellMultiBonus = false;
  u8 loopCombo = 0;
  bool kickback = false, ripBlinking = false;
  bool lockReady = false, lockWellReady = false, lockVaultReady = false;
  u8 lightPhaseRight = 0, lightPhaseTower = 0;
  u16 timeoutTopLoop = 0, timeoutLeftRamp = 0, timeoutMultiBonus = 0, timeoutLoopCombo = 0, timeoutTowerHunt = 0,
      timeoutLock = 0;
  Bcd scoreVault = Bcd::of("500000"), scoreWell = Bcd::of("100000"), scoreTowerBonus = Bcd::of("1000000");
};

/// What a player keeps between balls.
struct PlayerState {
  Bcd scoreMain, scoreBonus;
  u16 numCyclone = 0;
  Bcd bcdNumCyclone;
  // Party Land
  std::array<bool, 4> partyLightPuke{};
  std::array<bool, 3> partyLightMad{};
  std::array<bool, 5> partyLightCrazy{}, partyLightParty{};
  Bcd partyScoreTunnelSkillShot, partyScoreCycloneSkillShot;
  // Speed Devils
  u8 speedCurGear = 0, speedCurSpeed = 0, speedCurPlace = 0, speedMaxPlace = 0, speedCarMods = 0;
  bool speedLightGoal = false;
  std::array<bool, 5> speedLightCarLit{}, speedLightCar{};
  // Billion Dollar Gameshow
  u8 showPrizeSets = 0;
  // Stones 'n Bones
  u8 stonesCurGhost = 0;
  bool stonesGhostActive = false;
  Bcd stonesScoreSkillShot;
  bool stonesKickback = false;
  std::array<bool, 3> stonesLightRip{};
  std::array<bool, 5> stonesLightStone{};
  std::array<bool, 4> stonesLightBone{};
};

}  // namespace pfr
