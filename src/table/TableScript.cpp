// The table script interpreter, dot-matrix drawing and mode timers (translated from pfr's
// src/table/{script,dm,mode}.rs and the script tasks in game.rs).
#include <algorithm>

#include "table/Table.h"

namespace pfr {

namespace {
std::vector<u8> bytes(std::string_view s) { return {s.begin(), s.end()}; }
}  // namespace

// ---- dot matrix ----------------------------------------------------------------------------

u8 Table::dmSubChar(u8 chr) const {
  using namespace special_chars;
  if (chr < 0x80) return chr;
  if (chr >= kHighScores && chr < kHighScores + 12) {
    const int i = (chr - kHighScores) / 3, c = (chr - kHighScores) % 3;
    return highScores_[static_cast<std::size_t>(i)].name[static_cast<std::size_t>(c)];
  }
  const auto digit = [](int v) { return static_cast<u8>('0' + v); };
  const int bm = bonusMultLate_, nc = numCyclone_, nt = numCycloneTarget_;
  switch (chr) {
    case kCurBall: return digit(curBall_);
    case kCurPlayer: return digit(curPlayer_);
    case kTotalPlayers: return digit(totalPlayers_);
    case kBonusMultL: return bm == 10 ? '1' : digit(bm);
    case kBonusMultL + 1: return bm == 10 ? '0' : ' ';
    case kBonusMultR: return bm == 10 ? '1' : ' ';
    case kBonusMultR + 1: return bm == 10 ? '0' : digit(bm);
    case kNumCyclones: return nc < 100 ? '_' : digit(nc / 100 % 10);
    case kNumCyclones + 1: return nc < 10 ? '_' : digit(nc / 10 % 10);
    case kNumCyclones + 2: return digit(nc % 10);
    case kNumCyclonesTarget: return nt < 100 ? '_' : digit(nt / 100 % 10);
    case kNumCyclonesTarget + 1: return nt < 10 ? '_' : digit(nt / 10 % 10);
    case kNumCyclonesTarget + 2: return digit(nt % 10);
    case kNumCyclonesTargetL: return nt < 10 ? digit(nt % 10) : nt < 100 ? digit(nt / 10 % 10) : digit(nt / 100 % 10);
    case kNumCyclonesTargetL + 1: return nt < 10 ? '_' : nt < 100 ? digit(nt % 10) : digit(nt / 10 % 10);
    case kNumCyclonesTargetL + 2: return nt < 100 ? '_' : digit(nt % 10);
    default: return ' ';
  }
}

void Table::dmPutChar(DmFont font, DmCoord pos, u8 chr) {
  chr = dmSubChar(chr);
  if (chr == ' ') return;
  const auto& fontMap = assets_.dmFonts[static_cast<std::size_t>(font)];
  const auto it = fontMap.find(chr);
  if (it == fontMap.end()) return;
  // LongMsg needs the whole cell height cleared for a blank.
  if (font == DmFont::H13 && chr == '_') {
    for (auto& row : dm_.pixels)
      for (int x = 0; x < 8; ++x) {
        const int dx = pos.x + x;
        if (dx >= 0 && dx < 160) row[static_cast<std::size_t>(dx)] = false;
      }
    return;
  }
  const int h = dmFontHeight(font);
  for (int y = 0; y < h; ++y) {
    const int dy = pos.y + y;
    if (dy < 0 || dy >= 16) continue;
    const u8 line = it->second[static_cast<std::size_t>(y)];
    for (int x = 0; x < 8; ++x) {
      const int dx = pos.x + x;
      if (dx < 0 || dx >= 160) continue;
      dm_.pixels[static_cast<std::size_t>(dy)][static_cast<std::size_t>(dx)] = ((line << x) & 0x80) != 0;
    }
  }
}

void Table::dmPutBcd(DmFont font, DmCoord pos, const Bcd& num, bool center) {
  if (center) pos.x = static_cast<i16>(pos.x - static_cast<int>(num.leadingZeros()) * 4);
  const auto ascii = num.toAscii();
  const int h = dmFontHeight(font);
  for (std::size_t i = 0; i < ascii.size(); ++i) {
    dmPutChar(font, pos, ascii[i]);
    if ((i == 2 || i == 5 || i == 8) && ascii[i] != ' ') {
      // The thousands separator: a small comma under the digit's right edge.
      auto set = [&](int y, int x) {
        if (y >= 0 && y < 16 && x >= 0 && x < 160) dm_.pixels[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = true;
      };
      set(pos.y + h, pos.x + 7);
      set(pos.y + h, pos.x + 8);
      set(pos.y + h + 1, pos.x + 6);
      set(pos.y + h + 1, pos.x + 7);
    }
    pos.x = static_cast<i16>(pos.x + 8);
  }
}

void Table::dmPuts(DmFont font, DmCoord pos, const std::vector<u8>& msg) {
  for (u8 c : msg) {
    dmPutChar(font, pos, c);
    pos.x = static_cast<i16>(pos.x + 8);
    if (pos.x >= 160) break;
  }
}

void Table::dmPuts(DmFont font, DmCoord pos, std::string_view msg) { dmPuts(font, pos, bytes(msg)); }

void Table::dmAnimFrame(u8 frame) {
  for (const auto& [pos, state] : assets_.animFrames[frame])
    if (pos.y >= 0 && pos.y < 16 && pos.x >= 0 && pos.x < 160)
      dm_.pixels[static_cast<std::size_t>(pos.y)][static_cast<std::size_t>(pos.x)] = state;
}

// ---- mode timers ------------------------------------------------------------------------------

void Table::modeCountHit() {
  if (inModeHit_) scoreModeHit_ += assets_.scoreModeHitIncr;
}

void Table::modeCountRamp() {
  if (!inModeRamp_) return;
  if (assets_.table == 2)
    scoreModeHit_ += assets_.scoreModeRampIncr;
  else
    scoreModeRamp_ += assets_.scoreModeRampIncr;
}

bool Table::modeFrame(ScriptScore which) {
  dmPutBcd(DmFont::H13, {16, 1}, which == ScriptScore::ModeHit ? scoreModeHit_ : scoreModeRamp_, false);
  if (timerStop_) return true;
  if (--modeTimeoutFrames_ != 0) return true;
  modeTimeoutFrames_ = hifps_ ? 71 : 60;
  if (modeTimeoutSecs_ == 0) return false;
  --modeTimeoutSecs_;
  switch (assets_.table) {
    case 0: partyModeCheck(); break;
    case 1: speedModeCheck(); break;
    case 2: showModeCheck(); break;
    default: stonesModeCheck(); break;
  }
  const std::vector<u8> msg = {modeTimeoutSecs_ < 10 ? static_cast<u8>('_') : static_cast<u8>('0' + modeTimeoutSecs_ / 10),
                               static_cast<u8>('0' + modeTimeoutSecs_ % 10)};
  dmPuts(DmFont::H11, {144, 2}, msg);
  return true;
}

// ---- script tasks -------------------------------------------------------------------------------

bool Table::runScriptTask(ScriptTask& t) {
  using S = ScriptTaskKind;
  switch (t.kind) {
    case S::Placeholder: return false;
    case S::Default:
      if (kbdState_ != KbdState::Main) return false;
      if (timerIdle_ == 720) {
        if (!atSpring_) {
          inIdle_ = true;
          startScript(ScriptBind::GameIdle);
        }
      } else {
        if (enterAttract_) {
          enterAttract_ = false;
          startScript(ScriptBind::Attract);
          return false;
        }
        if (inAttract_) {
          startScript(ScriptBind::GameOver);
        } else if (!inPlunger_ && needDefaultBg_) {
          needDefaultBg_ = false;
          runUop(*assets_.scriptBinds[static_cast<std::size_t>(ScriptBind::Main)]);
        }
        ++timerIdle_;
      }
      checkTopScore();
      dmPutBcd(DmFont::H13, {64, 1}, scoreMain_, false);
      return false;
    case S::Delay: return --t.count != 0;
    case S::Halt: return true;
    case S::ConfirmQuit: return kbdState_ == KbdState::ConfirmQuit || quitting_;
    case S::WaitJingle: return sequencer_->jinglePlaying();
    case S::WaitWhileGameStarting: return inGameStart_;
    case S::AccBonus: return runAccBonus(t);
    case S::Mode: return modeFrame(t.mode);
    case S::DmClear: dm_.clear(); return false;
    case S::DmWipeDown:
      if (t.count == 16) return false;
      dm_.pixels[t.count++] = {};
      return true;
    case S::DmWipeRight:
      if (t.count == 160) return false;
      for (auto& row : dm_.pixels) row[t.count] = row[t.count + 1u] = false;
      t.count = static_cast<u16>(t.count + 2);
      return true;
    case S::DmWipeDownStriped:
      if (t.count == 4) return false;
      for (int k = 0; k < 4; ++k) dm_.pixels[t.count + 4u * static_cast<unsigned>(k)] = {};
      ++t.count;
      return true;
    case S::DmMsgScroll:
      t.pos = static_cast<i16>(t.down ? t.pos + 1 : t.pos - 1);
      dm_.clear();
      dmPuts(DmFont::H13, {0, t.pos}, assets_.msgs[t.msg]);
      return t.pos != static_cast<i16>(t.target);
    case S::DmLongMsg: {
      const auto& msg = assets_.msgs[t.msg];
      if (t.count + 20u >= msg.size()) return false;
      const std::vector<u8> rest(msg.begin() + t.count, msg.end());
      dmPuts(DmFont::H13, {t.pos, 1}, rest);
      --t.pos;
      dmPuts(DmFont::H13, {t.pos, 1}, rest);
      --t.pos;
      if (t.pos == -8) {
        t.pos = 0;
        ++t.count;
      }
      return true;
    }
    case S::DmAnim: {
      if (--t.delay != 0) return true;
      const DmAnim& anim = assets_.anims[t.anim];
      const std::size_t frameIdx = t.frameIdx;
      if (t.frameIdx == anim.numFrames) {
        if (--t.repeats == 0) return false;
        t.frameIdx = anim.restart;
      }
      const auto [frame, delay] = anim.frames[frameIdx];
      ++t.frameIdx;
      dmAnimFrame(frame);
      t.delay = delay;
      return true;
    }
    case S::DmTowerHunt:
      --t.count;
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 160; ++x)
          dm_.pixels[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = (*assets_.dmTower)(x, t.count + y) != 0;
      return t.target != t.count;
    case S::Match: return runMatch(t);
    case S::MatchStones: return runMatchStones(t);
    case S::RecordHighScores:
      if (curPlayer_ > totalPlayers_) {
        if (!gotHighScore_) playJingleBindForce(JingleBind::GameOverSad);
        return false;
      } else {
        const Bcd s = players_[curPlayer_ - 1u].scoreMain;
        for (std::size_t place = 0; place < 4; ++place)
          if (s > highScores_[place].score) {
            if (!gotHighScore_) {
              playJingleBindForce(JingleBind::GameOverHighScore);
              gotHighScore_ = true;
            }
            // The name goes in the three cells between the brackets, drawn over them below.
            std::vector<u8> msg = bytes("HIGHSCORE PL \x94 (   )");
            dmPuts(DmFont::H13, {0, 1}, msg);
            t = ScriptTask{};
            t.kind = S::RecordHighScoresGetName;
            t.target = static_cast<u16>(place);
            kbdState_ = KbdState::GetName;
            nameBuf_.clear();
            return true;
          }
        ++curPlayer_;
        return true;
      }
    case S::RecordHighScoresGetName: {
      const std::vector<u8> name = nameBuf_;
      dmPuts(DmFont::H13, {160 - 4 * 8, 1}, name);
      if (name.size() == 3) {
        const std::size_t place = t.target;
        HighScore h{players_[curPlayer_ - 1u].scoreMain, {name[0], name[1], name[2]}};
        for (std::size_t i = 3; i > place; --i) highScores_[i] = highScores_[i - 1];
        highScores_[place] = h;
        ++curPlayer_;
        flushHighScores_ = true;
        t = ScriptTask{};
        t.kind = S::RecordHighScoresFinish;
        t.count = 60;
      }
      return true;
    }
    case S::RecordHighScoresFinish:
      --t.count;
      if (t.count == 30) dm_.clear();
      if (t.count == 2) {
        t = ScriptTask{};
        t.kind = S::RecordHighScores;
      }
      return true;
  }
  return false;
}

void Table::scriptFrame() {
  ScriptTask task = scriptTask_;
  scriptTask_.kind = ScriptTaskKind::Placeholder;
  if (runScriptTask(task)) {
    if (scriptTask_.kind == ScriptTaskKind::Placeholder) scriptTask_ = task;
  } else {
    runUop(scriptPos_);
  }
}

void Table::startScript(ScriptBind bind) {
  const auto& pos = assets_.scriptBinds[static_cast<std::size_t>(bind)];
  if (pos) startScriptRaw(*pos);
}

void Table::startScriptRaw(u16 pos) {
  dm_.stopBlink();
  needDefaultBg_ = true;
  timerIdle_ = 0;
  runUop(pos);
}

Bcd Table::scriptScore(const ScriptScoreRef& s) const {
  switch (s.kind) {
    case ScriptScore::Bonus: return scoreBonus_;
    case ScriptScore::ModeHit: return scoreModeHit_;
    case ScriptScore::ModeRamp: return scoreModeRamp_;
    case ScriptScore::Jackpot: return scoreJackpot_;
    case ScriptScore::HighScore: return highScores_[s.arg].score;
    case ScriptScore::Const: return s.value;
    case ScriptScore::CycloneIncr: return Bcd::of("100000");
    case ScriptScore::NumCyclone: return bcdNumCyclone_;
    case ScriptScore::CycloneBonus: return scoreCycloneBonus_;
    case ScriptScore::PartyTunnelSkillShot: return party_.scoreTunnelSkillShot;
    case ScriptScore::PartyCycloneSkillShot: return party_.scoreCycloneSkillShot;
    case ScriptScore::ShowRaisingMillions: return scoreRaisingMillions_;
    case ScriptScore::ShowSpinWheel: return showWheelScore();
    case ScriptScore::ShowCashpot: return show_.scoreCashpot;
    case ScriptScore::ShowCashpotX5: {
      Bcd r;
      for (int i = 0; i < 5; ++i) r += show_.scoreCashpot;
      return r;
    }
    case ScriptScore::StonesSkillShot: return stones_.scoreSkillShot;
    case ScriptScore::StonesMillionPlus: return stones_.scoreMillionPlus;
    case ScriptScore::StonesVault: return stones_.scoreVault;
    case ScriptScore::StonesWell: return stones_.scoreWell;
    case ScriptScore::StonesTowerBonus: return stones_.scoreTowerBonus;
    default: return Bcd::kZero;
  }
}

void Table::runUop(u16 pos) {
  using S = ScriptTaskKind;
  scriptPos_ = static_cast<u16>(pos + 1);
  const Uop u = assets_.scripts[pos];
  auto task = [&](S kind) {
    scriptTask_ = ScriptTask{};
    scriptTask_.kind = kind;
  };
  auto delay = [&](u16 n) {
    task(S::Delay);
    scriptTask_.count = n;
  };
  auto bind = [&](ScriptBind b) { return *assets_.scriptBinds[static_cast<std::size_t>(b)]; };
  switch (u.kind) {
    case UopKind::End:
      dm_.stopBlink();
      task(S::Default);
      scriptPos_ = pos;
      break;
    case UopKind::Noop: delay(1); break;
    case UopKind::Delay: delay(u.value); break;
    case UopKind::DelayIfMultiplayer: delay(totalPlayers_ != 1 ? u.value : 2); break;
    case UopKind::Halt: task(S::Halt); break;
    case UopKind::Jump: runUop(u.target); break;
    case UopKind::JccScoreZero:
      if (scriptScore(u.score).isZero())
        runUop(u.target);
      else
        runUop(scriptPos_);
      break;
    case UopKind::JccNoBonusMult:
      if (bonusMultLate_ == 1)
        runUop(u.target);
      else
        delay(1);
      break;
    case UopKind::RepeatSetup:
      delay(1);
      repeatCnt_ = u.value;
      break;
    case UopKind::RepeatLoop:
      delay(1);
      if (--repeatCnt_ == 0)
        repeatCnt_ = u.value;
      else
        runUop(u.target);
      break;
    case UopKind::FinalScoreSetup:
      delay(1);
      curPlayer_ = 1;
      break;
    case UopKind::FinalScoreLoop:
      delay(1);
      dmPuts(DmFont::H5, {0, 1}, "PLAYER \x96");
      dmPutBcd(DmFont::H13, {64, 1}, players_[curPlayer_ - 1u].scoreMain, false);
      if (curPlayer_ != totalPlayers_) {
        ++curPlayer_;
        scriptPos_ = u.target;
      }
      break;
    case UopKind::ConfirmQuit: task(S::ConfirmQuit); break;
    case UopKind::WaitWhileGameStarting: task(S::WaitWhileGameStarting); break;
    case UopKind::ExtraBall:
      delay(1);
      extraBall();
      break;
    case UopKind::SetupPartyOn:
      delay(1);
      specialPlungerEvent_ = true;
      break;
    case UopKind::SetupShootAgain: delay(1); break;
    case UopKind::SetSpecialPlungerEvent:
      task(S::Halt);
      specialPlungerEvent_ = true;
      break;
    case UopKind::IssueBall:
      addTask(TaskKind::IssueBall);
      runUop(scriptPos_);
      break;
    case UopKind::MultiplyBonus: {
      delay(1);
      const Bcd bonus = scoreBonus_;
      for (int i = 1; i < bonusMultLate_; ++i) scoreBonus_ += bonus;
      break;
    }
    case UopKind::AccBonusModeHit:
      delay(1);
      scoreBonus_ += scoreModeHit_;
      break;
    case UopKind::AccBonusModeRamp:
      delay(1);
      scoreBonus_ += scoreModeRamp_;
      break;
    case UopKind::AccBonusCyclones:
      delay(1);
      scoreCycloneBonus_ = Bcd::kZero;
      for (int i = 0; i < numCyclone_; ++i) scoreCycloneBonus_ += Bcd::of("100000");
      scoreBonus_ += scoreCycloneBonus_;
      break;
    case UopKind::AccBonus:
      task(S::AccBonus);
      scriptTask_.frame = 0;
      scriptTask_.digitIdx = 11;
      scriptTask_.score = scoreBonus_;
      break;
    case UopKind::CheckTopScore:
      if (!gotTopScore_ && scoreMain_ > highScores_[0].score) {
        gotTopScore_ = true;
        runUop(bind(ScriptBind::TopScoreInterball));
      } else {
        runUop(scriptPos_);
      }
      break;
    case UopKind::NextBallIfMatched:
      if (matchDigit_) {
        saveCurPlayer();
        if (extraBalls_ != 0) {
          --extraBalls_;
          runUop(bind(ScriptBind::ShootAgain));
        } else if (curPlayer_ != totalPlayers_) {
          ++curPlayer_;
          runUop(bind(ScriptBind::CheckMatch));
        } else {
          runUop(bind(ScriptBind::PostMatch));
        }
      } else {
        runUop(scriptPos_);
      }
      break;
    case UopKind::NextBall:
      if (!holdBonus_) scoreBonus_ = Bcd::kZero;
      saveCurPlayer();
      if (extraBalls_ != 0) {
        --extraBalls_;
        runUop(bind(ScriptBind::ShootAgain));
      } else if (curPlayer_ != totalPlayers_) {
        ++curPlayer_;
        addTask(TaskKind::IssueBall);
        runUop(scriptPos_);
      } else if (curBall_ != totalBalls_) {
        ++curBall_;
        curPlayer_ = 1;
        addTask(TaskKind::IssueBall);
        runUop(scriptPos_);
      } else {
        runUop(bind(ScriptBind::Match));
      }
      break;
    case UopKind::Match: {
      curPlayer_ = 1;
      playJingleBindSilence(JingleBind::MatchStart);
      for (std::size_t i = 0; i < players_.size(); ++i) {
        const u8 d = players_[i].scoreMain.digits[10];
        dmPuts(DmFont::H5, {static_cast<i16>(i * 16), 0}, std::vector<u8>{static_cast<u8>('0' + d)});
      }
      const u8 digit = static_cast<u8>(rand(10));
      if (assets_.table == 3) {
        task(S::MatchStones);
        scriptTask_.count = matchTiming_[0];
        scriptTask_.frameIdx = 0;
      } else {
        task(S::Match);
        static constexpr u16 kCount[3] = {22, 18, 15};
        const u16 frames = assets_.table == 0 ? (hifps_ ? 11 : 9) : assets_.table == 1 ? (hifps_ ? 13 : 11) : 14;
        scriptTask_.count = kCount[assets_.table];
        scriptTask_.pos = static_cast<i16>(frames);
        scriptTask_.target = frames;
      }
      scriptTask_.digit = digit;
      break;
    }
    case UopKind::CheckMatch: {
      bool found = false;
      for (std::size_t i = 0; i < players_.size(); ++i)
        if (matchDigit_ == players_[i].scoreMain.digits[10]) {
          curPlayer_ = static_cast<u8>(i + 1);
          runUop(bind(ScriptBind::ShootAgain));
          found = true;
          break;
        }
      if (!found) runUop(bind(ScriptBind::PostMatch));
      break;
    }
    case UopKind::RecordHighScores:
      curPlayer_ = 1;
      task(S::RecordHighScores);
      break;
    case UopKind::GameOver:
      addTask(TaskKind::GameOver);
      // As in the DOS original, the script does not continue past this point.
      task(S::Default);
      break;
    case UopKind::DmState:
      delay(1);
      dm_.state = u.flag;
      break;
    case UopKind::DmBlink:
      delay(1);
      dm_.startBlink(u.value);
      break;
    case UopKind::DmStopBlink:
      delay(1);
      dm_.stopBlink();
      break;
    case UopKind::DmClear: task(S::DmClear); break;
    case UopKind::DmWipeDown: task(S::DmWipeDown); break;
    case UopKind::DmWipeRight: task(S::DmWipeRight); break;
    case UopKind::DmWipeDownStriped: task(S::DmWipeDownStriped); break;
    case UopKind::DmAnim:
      task(S::DmAnim);
      scriptTask_.anim = u.anim;
      scriptTask_.frameIdx = 0;
      scriptTask_.delay = 1;
      scriptTask_.repeats = assets_.anims[u.anim].repeats;
      break;
    case UopKind::DmPuts:
      delay(1);
      dmPuts(u.font, u.coord, assets_.msgs[u.msg]);
      break;
    case UopKind::DmPrintScore:
      delay(1);
      if (u.score.kind == ScriptScore::ShowSpinWheel) dm_.clear();
      dmPutBcd(u.font, u.coord, scriptScore(u.score), u.flag);
      break;
    case UopKind::DmMsgScrollUp:
    case UopKind::DmMsgScrollDown:
      task(S::DmMsgScroll);
      scriptTask_.msg = u.msg;
      scriptTask_.down = u.kind == UopKind::DmMsgScrollDown;
      scriptTask_.pos = scriptTask_.down ? -13 : 16;
      scriptTask_.target = static_cast<u16>(u.end);
      break;
    case UopKind::DmLongMsg:
      task(S::DmLongMsg);
      scriptTask_.msg = u.msg;
      scriptTask_.count = 0;
      scriptTask_.pos = 0;
      break;
    case UopKind::DmTowerHunt:
      task(S::DmTowerHunt);
      scriptTask_.target = u.value;
      scriptTask_.count = 152;
      break;
    case UopKind::SetJingleTimeout: delay(1); break;
    case UopKind::WaitJingle:
    case UopKind::WaitJingleTimeout: task(S::WaitJingle); break;
    case UopKind::PlayJingle:
      delay(1);
      sequencer_->playJingle(u.jingle, true, std::nullopt);
      break;
    case UopKind::PlaySfx:
      delay(1);
      player_->playSfx(u.sfx, u.volume);
      break;
    case UopKind::SetMusic:
      delay(1);
      sequencer_->setMusic(static_cast<u8>(u.value));
      break;
    case UopKind::ModeContinue:
      pendingMode_ = false;
      modeTimeoutFrames_ = 1;
      task(S::Mode);
      scriptTask_.mode = u.score.kind;
      break;
    case UopKind::ModeStart:
      pendingMode_ = false;
      modeTimeoutSecs_ = static_cast<u8>(u.time + 1);
      modeTimeoutFrames_ = 1;
      task(S::Mode);
      scriptTask_.mode = u.score.kind;
      break;
    case UopKind::ModeStartOrContinue:
      if (pendingMode_) {
        pendingMode_ = false;
        modeTimeoutSecs_ = static_cast<u8>(u.time + 1);
      }
      modeTimeoutFrames_ = 1;
      task(S::Mode);
      scriptTask_.mode = u.score.kind;
      break;
    case UopKind::PartySecretDrop:
      delay(1);
      party_.secretDropRelease = true;
      break;
    case UopKind::PartyArcadeReady:
      delay(1);
      party_.arcadeReady = true;
      break;
    case UopKind::SpeedCheckTurboCont:
      delay(1);
      if (timerStop_) {
        timerStop_ = false;
        runUop(bind(ScriptBind::SpeedModeRampContinue));
      }
      break;
    case UopKind::SpeedClearFlagMode:
      delay(1);
      inMode_ = false;
      sequencer_->resetPriority();
      break;
    case UopKind::SpeedStartTurbo:
      lightSet(LightBind::SpeedPitStopGoal, 0, false);
      speedDoTurbo();
      runUop(bind(ScriptBind::SpeedModeRamp));
      break;
    case UopKind::ShowBlinkMoneyMania:
      delay(1);
      lightBlink(LightBind::ShowMoneyMania, 0, 3, 0);
      break;
    case UopKind::ShowEndMoneyMania:
      delay(1);
      lightSet(LightBind::ShowMoneyMania, 0, false);
      inMode_ = false;
      sequencer_->resetPriority();
      break;
    case UopKind::ShowSpinWheelEnd:
      delay(1);
      addTask(TaskKind::ShowSpinWheelEnd);
      break;
    case UopKind::StonesTowerEject:
      delay(1);
      addTask(TaskKind::StonesTowerEject);
      break;
    case UopKind::StonesVaultEject:
      delay(1);
      addTask(TaskKind::StonesVaultEject);
      stones_.vaultHold = false;
      break;
    case UopKind::StonesWellEject:
      delay(1);
      addTask(TaskKind::StonesWellEject);
      break;
    case UopKind::StonesTiltEject:
      delay(1);
      if (stones_.inVault)
        addTask(TaskKind::StonesVaultEject);
      else if (stones_.inTower)
        addTask(TaskKind::StonesTowerEject);
      break;
    case UopKind::StonesSetFlagMode: delay(1); inMode_ = true; break;
    case UopKind::StonesSetFlagModeRamp: delay(1); inModeRamp_ = true; break;
    case UopKind::StonesSetFlagModeHit: delay(1); inModeHit_ = true; break;
    case UopKind::StonesClearFlagMode:
      delay(1);
      inMode_ = false;
      sequencer_->resetPriority();
      break;
    case UopKind::StonesClearFlagModeRamp: delay(1); inModeRamp_ = false; break;
    case UopKind::StonesClearFlagModeHit: delay(1); inModeHit_ = false; break;
    case UopKind::StonesEndMode:
      delay(1);
      stonesEndMode();
      break;
    case UopKind::StonesEndGrimReaper:
      delay(1);
      lightSet(LightBind::StonesGhost, 7, false);
      break;
    default: delay(1); break;
  }
}

void Table::checkTopScore() {
  if (inPlunger_ || inMode_ || gotTopScore_) return;
  if (scoreMain_ > highScores_[0].score) {
    gotTopScore_ = true;
    startScript(ScriptBind::TopScoreIngame);
  }
}

void Table::resetIdle() {
  timerIdle_ = 0;
  if (inIdle_) {
    startScript(ScriptBind::Main);
    inIdle_ = false;
  }
}

// ---- bonus count and match (game.rs) ----------------------------------------------------------

bool Table::runAccBonus(ScriptTask& t) {
  if (++t.frame != 4) return true;
  t.frame = 0;
  while (t.score.digits[t.digitIdx] == 0) {
    if (t.digitIdx == 0) {
      dmPuts(DmFont::H11, {-32, 6}, "___________");
      return false;
    }
    --t.digitIdx;
  }
  --t.score.digits[t.digitIdx];
  if (t.score.digits[t.digitIdx] == 0 && !t.score.isZero()) t.frame = -10;
  Bcd delta;
  delta.digits[t.digitIdx] = 1;
  scoreMain_ += delta;
  playSfxBind(SfxBind::TickBonus);
  dmPutBcd(DmFont::H8, {-32, 6}, t.score, false);
  dmPutBcd(DmFont::H13, {64, 1}, scoreMain_, false);
  return true;
}

bool Table::runMatch(ScriptTask& t) {
  t.pos = static_cast<i16>(t.pos - 1);
  if (t.pos != 0) return true;
  t.pos = static_cast<i16>(t.target);
  dmPuts(DmFont::H5, {static_cast<i16>(t.digit * 16), 7}, "_");
  u8 d = static_cast<u8>(rand(10));
  if (d == t.digit && ++d == 10) d = 0;
  t.digit = d;
  dmPuts(DmFont::H5, {static_cast<i16>(t.digit * 16), 7}, std::vector<u8>{static_cast<u8>('0' + t.digit)});
  if (--t.count == 0) {
    matchDone(t.digit);
    return false;
  }
  return true;
}

bool Table::runMatchStones(ScriptTask& t) {
  if (--t.count != 0) return true;
  t.count = matchTiming_[t.frameIdx];
  ++t.frameIdx;
  dmPuts(DmFont::H5, {static_cast<i16>(t.digit * 16), 7}, "_");
  t.digit = t.digit == 0 ? 9 : static_cast<u8>(t.digit - 1);
  dmPuts(DmFont::H5, {static_cast<i16>(t.digit * 16), 7}, std::vector<u8>{static_cast<u8>('0' + t.digit)});
  if (t.frameIdx == matchTiming_.size()) {
    matchDone(t.digit);
    return false;
  }
  return true;
}

}  // namespace pfr
