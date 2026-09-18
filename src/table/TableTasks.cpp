// Delayed actions (translated from pfr's src/table/tasks.rs). Each table's own task kinds
// run in that table's file.
#include "table/Table.h"

namespace pfr {

void Table::addTask(TaskKind kind, u16 a, u16 b, bool flag) { tasks_.push_back(Task{kind, a, b, flag, 0}); }

void Table::tasksFrame() {
  std::vector<Task> running = std::move(tasks_);
  tasks_.clear();
  std::vector<Task> kept;
  for (Task& t : running)
    if (runTask(t)) kept.push_back(t);
  // Tasks added while running come first, as in pfr.
  tasks_.insert(tasks_.end(), kept.begin(), kept.end());
}

bool Table::runTask(Task& t) {
  if (t.timer != taskDelay(t)) {
    ++t.timer;
    return true;
  }
  switch (t.kind) {
    case TaskKind::SetStartKeysActive: startKeysActive_ = true; return false;
    case TaskKind::PartyOn:
      partyOn_ = true;
      issueBall();
      return false;
    case TaskKind::IssueBall: issueBall(); return false;
    case TaskKind::IssueBallFinish: issueBallFinish(); return false;
    case TaskKind::IssueBallRelease: issueBallRelease(); return false;
    case TaskKind::IssueBallSfx: playSfxBind(SfxBind::IssueBall); return false;
    case TaskKind::IssueBallRaiseSfx: playSfxBind(SfxBind::RaiseHitTargets); return false;
    case TaskKind::DrainSfx: playSfxBind(SfxBind::BallDrained); return false;
    case TaskKind::GameOver:
      kbdState_ = KbdState::Main;
      inAttract_ = true;
      lightsReset();
      startKeysActive_ = true;
      scoreMain_ = Bcd::kZero;
      if (assets_.table == 0) lightSetAll(LightBind::PartyDuckDrop, true);
      return false;
    default: break;
  }
  if (t.kind <= TaskKind::PartyMegaLaugh) return runPartyTask(t);
  if (t.kind <= TaskKind::SpeedResetSuperJackpot) return runSpeedTask(t);
  if (t.kind <= TaskKind::ShowCashpotEject) return runShowTask(t);
  return runStonesTask(t);
}

u16 Table::taskDelay(const Task& t) const {
  switch (t.kind) {
    case TaskKind::SetStartKeysActive: return 15;
    case TaskKind::PartyOn: return 30;
    case TaskKind::IssueBall: return 30;
    case TaskKind::IssueBallFinish: return 30;
    case TaskKind::IssueBallRelease: return 80;
    case TaskKind::IssueBallSfx: return 45;
    case TaskKind::IssueBallRaiseSfx: return 5;
    case TaskKind::DrainSfx: return 5;
    case TaskKind::GameOver: return 0;
    case TaskKind::PartyDropZoneStart: return t.a;
    case TaskKind::PartyDropZoneWait: return 30;
    case TaskKind::PartyDropZoneRelease: return 27;
    case TaskKind::PartyDropZoneScroll: return 0;
    case TaskKind::PartyOrbitRightUnblink: return 120;
    case TaskKind::PartyMadUnblink: return 14;
    case TaskKind::PartyMadAllUnblink: return 120;
    case TaskKind::PartySecretDrop: return 0;
    case TaskKind::PartyCycloneX5Blink: return 480;
    case TaskKind::PartyCycloneX5End: return 120;
    case TaskKind::PartyTunnelFreeze: return 2;
    case TaskKind::PartyArcadePickReward: return 0;
    case TaskKind::PartyArcadeDropZoneStart: return 0;
    case TaskKind::PartyDoubleBonusBlink: return 480;
    case TaskKind::PartyDoubleBonusEnd: return 120;
    case TaskKind::PartySnacksRelease: return inMode_ ? 40 : 130;
    case TaskKind::PartySnacksFinish: return 60;
    case TaskKind::PartyDemonBlink: return t.a;
    case TaskKind::PartyDemonRelease: return 27;
    case TaskKind::PartySideExtraBallFinish: return 600;
    case TaskKind::PartySkyrideUnblink: return 120;
    case TaskKind::PartyPukeUnblink: return 13;
    case TaskKind::PartyPukeUnblinkAll: return 100;
    case TaskKind::PartyResetArcadeButton: return 20;
    case TaskKind::PartyDuckDrop: return 20;
    case TaskKind::PartyDuckUnblink: return 13;
    case TaskKind::PartyDuckAllUnblink: return 71;
    case TaskKind::PartyHappyHour: return 400;
    case TaskKind::PartyMegaLaugh: return 400;
    case TaskKind::SpeedUnblinkBur: return 10;
    case TaskKind::SpeedUnblinkBurAll: return 40;
    case TaskKind::SpeedUnblinkNin: return 10;
    case TaskKind::SpeedUnblinkNinAll: return 40;
    case TaskKind::SpeedUnblinkGear: return 10;
    case TaskKind::SpeedUnblinkGearAll: return 45;
    case TaskKind::SpeedTurbo: return 0;
    case TaskKind::SpeedOffroad: return 0;
    case TaskKind::SpeedPitStop: return t.a;
    case TaskKind::SpeedUnblinkCar: return 120;
    case TaskKind::SpeedResetSuperJackpot: return 1200;
    case TaskKind::ShowResetDropCenter: return 60;
    case TaskKind::ShowResetDropLeft: return 60;
    case TaskKind::ShowUnblinkDollar: return 25;
    case TaskKind::ShowUnblinkDollarAll: return 60;
    case TaskKind::ShowVaultEject: return 30;
    case TaskKind::ShowBillionRelease: return 250;
    case TaskKind::ShowSpinWheelEnd: return 100;
    case TaskKind::ShowGivePrize: return 0;
    case TaskKind::ShowCashpot: return 160;
    case TaskKind::ShowCashpotEject: return t.a;
    case TaskKind::StonesUnblinkStone: return 10;
    case TaskKind::StonesUnblinkBone: return 10;
    case TaskKind::StonesUnblinkStonesBones: return 70;
    case TaskKind::StonesUnblinkKey: return 10;
    case TaskKind::StonesUnblinkKeyAll: return 70;
    case TaskKind::StonesResetSuperJackpot: return 780;
    case TaskKind::StonesTowerEject: return 10;
    case TaskKind::StonesTowerEjectNow: return 0;
    case TaskKind::StonesWellEject: return 10;
    case TaskKind::StonesVaultEject: return 10;
    case TaskKind::StonesUnblinkGhosts: return 240;
    case TaskKind::StonesModeHit: return 0;
    case TaskKind::StonesModeRamp: return 0;
    case TaskKind::StonesRaiseKickback: return 30;
    case TaskKind::StonesUnblinkRip: return 20;
    case TaskKind::StonesUnblinkRipAll: return 70;
    case TaskKind::StonesScreamExtra: return 2;
  }
  return 0;
}

}  // namespace pfr
