#include "table/Table.h"

#include <algorithm>

namespace pfr {

namespace {

constexpr std::array<std::array<i16, 5>, 8> kMaterials = {{
    {1792, 448, 400, 300, 38},    // 0: dummy
    {1792, 448, 400, 600, 18},    // 1: dummy
    {1792, 448, 400, 600, 18},    // 2: flipper, patch
    {896, 224, 875, 200, 38},     // 3: rubber (kickers)
    {1792, 448, 400, 300, 38},    // 4: dummy
    {30000, 7500, 1000, 400, 38}, // 5: dummy
    {10000, 2500, 450, 700, 38},  // 6: steel
    {10000, 2500, 400, 500, 38},  // 7: plastic (bumpers)
}};

constexpr std::array<u16, 36> kMatchTimingLow = {24, 23, 21, 21, 18, 16, 15, 13, 11, 9, 8, 7, 7, 6, 6, 6, 5, 5,
                                                 5,  5,  5,  4,  4,  4,  4,  4,  4,  4, 4, 4, 3, 3, 3, 3, 3, 3};
constexpr std::array<u16, 36> kMatchTimingHigh = {22, 28, 25, 25, 22, 19, 18, 15, 13, 11, 9, 9, 8, 8, 7, 7, 6, 6,
                                                  6,  6,  6,  5,  5,  5,  5,  5,  5,  4,  4, 4, 4, 4, 4, 4, 3, 3};

}  // namespace

Table::Table(ByteView prg, ByteView module, const Config& config, int table, u64 seed)
    : assets_(TableAssets::load(prg, table)),
      options_(config.options),
      highScores_(config.highScores[static_cast<std::size_t>(table)]),
      rng_(seed),
      show_(false) {
  sequencer_ = std::make_shared<TableSequencer>(jingle(JingleBind::Attract).position, assets_.positionJingleStart,
                                                jingle(JingleBind::Silence).position, options_.noMusic);
  player_ = std::make_unique<Player>(Mod::load(module), sequencer_);

  scroll_.speed = rawScrollSpeed(options_.scrollSpeed);
  scroll_.setResolution(options_.resolution, std::nullopt);
  scroll_.pos = static_cast<u16>(576 - scroll_.windowHeight);
  scroll_.rawPosF4 = 0;
  lights_.resize(assets_.lights.size());
  attractCtr_.assign(assets_.attractLights.size(), 0);
  for (const Flipper& f : assets_.flippers) {
    FlipperState s;
    s.accelPress = speedFix(f.accelPress);
    s.accelRelease = speedFix(f.accelRelease);
    s.speedPressStart = speedFix(f.speedPressStart);
    flippers_.push_back(s);
  }
  physmaps_ = assets_.physmaps;
  for (std::size_t i = 0; i < 8; ++i) {
    const auto& m = kMaterials[i];
    materials_[i] = {m[0], m[1], m[2], speedFix(m[3]), m[4]};
  }
  kickerSpeedThreshold_ = speedFix(300);
  kickerSpeedBoost_ = speedFix(2000);
  bumperSpeedBoost_ = speedFix(7000);
  matchTiming_ = hifps_ ? kMatchTimingHigh : kMatchTimingLow;
  push_.speedAttack = speedFix(600);
  push_.speedRelease = speedFix(-200);
  ball_.maxSpeed = speedFix(4100);
  totalBalls_ = options_.balls;

  ball_.setPos({280, 525});
  startScript(ScriptBind::Init);
  flippersPhysmapUpdate();
}

Table::~Table() = default;

int Table::screenHeight() const {
  switch (options_.resolution) {
    case Resolution::Normal: return 240;
    case Resolution::High: return 350;
    case Resolution::Full: return 576 + 33;
  }
  return 240;
}

// ---- pause and options (table.rs) ---------------------------------------------------------

void Table::pause() {
  dm_.saved = dm_.pixels;
  dm_.clear();
  dm_.state = true;
  dmPuts(DmFont::H13, {36, 1}, "GAME PAUSED");
  kbdState_ = KbdState::Paused;
  pauseCycle_ = 0;
  player_->pause();
}

void Table::unpause() {
  dm_.pixels = dm_.saved;
  kbdState_ = KbdState::Main;
  player_->unpause();
}

void Table::toggleMusic() {
  if (options_.noMusic) {
    options_.noMusic = false;
    sequencer_->setMusic(jingle(inPlunger_ ? JingleBind::Plunger : JingleBind::Main).position);
    sequencer_->forceEndLoop();
  } else {
    options_.noMusic = true;
    playJingleBindForce(JingleBind::Silence);
  }
  sequencer_->setNoMusic(options_.noMusic);
}

void Table::pauseOptionAngle() {
  options_.angleHigh = !options_.angleHigh;
  dm_.clear();
  if (options_.angleHigh)
    dmPuts(DmFont::H13, {40, 1}, "ANGLE HIGH");
  else
    dmPuts(DmFont::H13, {44, 1}, "ANGLE LOW");
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseOptionScrolling() {
  options_.scrollSpeed = options_.scrollSpeed == ScrollSpeed::Hard     ? ScrollSpeed::Medium
                         : options_.scrollSpeed == ScrollSpeed::Medium ? ScrollSpeed::Soft
                                                                       : ScrollSpeed::Hard;
  scroll_.speed = rawScrollSpeed(options_.scrollSpeed);
  dm_.clear();
  switch (options_.scrollSpeed) {
    case ScrollSpeed::Hard: dmPuts(DmFont::H13, {24, 1}, "SCROLLING HARD"); break;
    case ScrollSpeed::Medium: dmPuts(DmFont::H13, {16, 1}, "SCROLLING MEDIUM"); break;
    case ScrollSpeed::Soft: dmPuts(DmFont::H13, {24, 1}, "SCROLLING SOFT"); break;
  }
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseOptionMusic() {
  toggleMusic();
  dm_.clear();
  if (options_.noMusic)
    dmPuts(DmFont::H13, {44, 1}, "MUSIC OFF");
  else
    dmPuts(DmFont::H13, {48, 1}, "MUSIC ON");
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseOptionResolution() {
  options_.resolution = options_.resolution == Resolution::Normal ? Resolution::High
                        : options_.resolution == Resolution::High ? Resolution::Full
                                                                  : Resolution::Normal;
  scroll_.setResolution(options_.resolution, inAttract_ ? std::nullopt : std::optional<i16>(ball_.pos()[1]));
  dm_.clear();
  dmPuts(DmFont::H13, {8, 1}, "RESOLUTION CHANGED");
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseConfirmQuit() {
  dm_.clear();
  dmPuts(DmFont::H13, {0, 1}, "REALLY QUIT (Y OR N)");
  kbdState_ = KbdState::PausedConfirmQuit;
}

// ---- the frame ---------------------------------------------------------------------------

TableAction Table::runFrame() {
  using K = TableAction::Kind;
  if (kbdState_ == KbdState::Paused) {
    ++pauseCycle_;
    if (pauseCycle_ == 120) {
      dm_.clear();
      dmPuts(DmFont::H13, {32, 1}, "P TO UNPAUSE");
    } else if (pauseCycle_ == 240) {
      dm_.clear();
      dmPuts(DmFont::H13, {16, 1}, "ASMR FOR OPTIONS");
    } else if (pauseCycle_ == 360) {
      dm_.clear();
      dmPuts(DmFont::H13, {36, 1}, "GAME PAUSED");
      pauseCycle_ = 0;
    }
    if (optionChanged_) {
      optionChanged_ = false;
      return {K::SaveOptions};
    }
    return {};
  }
  if (kbdState_ == KbdState::PausedConfirmQuit) return {};
  if (quitting_) {
    if (fade_ != 0) fade_ = static_cast<u16>(fade_ - 2);
    player_->setMasterVolume(fade_);
    return fade_ == 0 ? TableAction{K::Quit} : TableAction{};
  }

  if (inAttract_) {
    scroll_.attractFrame();
    lightsAttractFrame();
    dm_.blinkFrame();
    if (startKey_) {
      const u8 players = *startKey_;
      startKey_.reset();
      totalPlayers_ = players;
      players_.assign(players, PlayerState{});
      startScript(ScriptBind::GameStart);
      playSfxBind(SfxBind::GameStart);
      inAttract_ = false;
      initGame();
      const Jingle& start = jingle(JingleBind::GameStart);
      const Jingle& plunger = jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Plunger);
      sequencer_->playJingle(start, true, plunger.position);
      issueBall();
      addTask(TaskKind::SetStartKeysActive);
    }
  } else {
    scroll_.update(ball_.pos()[1]);
    if (startKey_) {
      const u8 players = *startKey_;
      startKey_.reset();
      totalPlayers_ = players;
      players_.assign(players, PlayerState{});
      startScript(ScriptBind::GameStartPlayers);
      playSfxBind(SfxBind::GameStart);
      addTask(TaskKind::SetStartKeysActive);
    }
    if (!cheatSlowdown_) physicsFrame();
    physicsFrame();
    physicsFrame();
    physicsFrame();
    if (tiltCounter_ != 0) --tiltCounter_;
    scoreBumper();
    ballGravity();
    checkTransitions();
    if (drained_ && !inDrain_) {
      ballTeleportFreeze(Layer::Ground, {280, 525});
      flippersEnabled_ = false;
      inMode_ = inModeHit_ = inModeRamp_ = false;
      if (!blockDrain_) {
        inDrain_ = true;
        switch (assets_.table) {
          case 0: partyDrained(); break;
          case 1: speedDrained(); break;
          case 2: showDrained(); break;
          default: stonesDrained(); break;
        }
      }
    }
    switch (assets_.table) {
      case 0: partyFrame(); break;
      case 1: speedFrame(); break;
      case 2: showFrame(); break;
      default: stonesFrame(); break;
    }
    doRollTriggers();
    doHitTriggers();
    if (flipperPressed_) {
      flipperPressed_ = false;
      switch (assets_.table) {
        case 0: partyFlipperPressed(); break;
        case 1: speedFlipperPressed(); break;
        case 2: showFlipperPressed(); break;
        default: stonesFlipperPressed(); break;
      }
    }
    if (spacePressed_) {
      spacePressed_ = false;
      if (!cheatNoTilt_ && !inPlunger_ && !drained_ && !tilted_) {
        tiltCounter_ = static_cast<u16>(tiltCounter_ + 60);
        if (tiltCounter_ > 120) {
          tilted_ = true;
          flippersEnabled_ = false;
          playJingleBindSilence(JingleBind::Tilt);
          startScript(ScriptBind::Tilt);
          lightsTilt();
          party_.secretDropRelease = true;
        } else if (tiltCounter_ > 60) {
          playJingleBind(JingleBind::WarnTilt);
        }
      }
    }
    dm_.blinkFrame();
    tasksFrame();
    lightsBlinkFrame();
    if (springReleased_ && springPos_ != 0) {
      springRelease();
      springReleased_ = false;
    } else if (springDownState_ && springPos_ < 0x20) {
      ++springPos_;
    }
  }
  scriptFrame();
  if (flushHighScores_) {
    flushHighScores_ = false;
    return {K::SaveHighScores};
  }
  if (optionChanged_) {
    optionChanged_ = false;
    return {K::SaveOptions};
  }
  return {};
}

void Table::handleKey(Key key, bool pressed) {
  auto flipperKey = [&](FlipperSide side) {
    const std::size_t s = static_cast<std::size_t>(side);
    if (pressed && flippersEnabled_ && !flipperState_[s]) {
      flipperPressed_ = true;
      playSfxBind(SfxBind::FlipperPress);
    }
    flipperState_[s] = pressed;
  };
  if (key == Key::ShiftLeft || key == Key::ControlLeft || key == Key::AltLeft) flipperKey(FlipperSide::Left);
  if (key == Key::ShiftRight || key == Key::ControlRight || key == Key::AltRight) flipperKey(FlipperSide::Right);
  if (key == Key::Space) {
    if (pressed && !spaceState_) spacePressed_ = true;
    spaceState_ = pressed;
  }
  if (key == Key::ArrowDown) {
    springDownState_ = pressed;
    if (!pressed) springReleased_ = true;
  }
  if (!pressed) return;
  const u8 chr = keyChar(key);

  switch (kbdState_) {
    case KbdState::Main:
      if (startKeysActive_ && (inAttract_ || atSpring_)) {
        if (key >= Key::F1 && key <= Key::F8)
          startKey_ = static_cast<u8>(static_cast<int>(key) - static_cast<int>(Key::F1) + 1);
        else if (key >= Key::Digit1 && key <= Key::Digit8)
          startKey_ = static_cast<u8>(static_cast<int>(key) - static_cast<int>(Key::Digit1) + 1);
        else if (key == Key::Enter) {
          if (inAttract_)
            startKey_ = 1;
          else if (totalPlayers_ < 8)
            startKey_ = static_cast<u8>(totalPlayers_ + 1);
        }
        if (startKey_) startKeysActive_ = false;
      }
      if (inAttract_) {
        if (chr) handleCheat(chr);
        if (key == Key::Escape) {
          kbdState_ = KbdState::ConfirmQuit;
          startScript(ScriptBind::ConfirmQuit);
        }
      } else if (!inDrain_) {
        if (key == Key::Escape && atSpring_) {
          abortGame();
        } else if (key == Key::M) {
          toggleMusic();
          optionChanged_ = true;
        } else if (key == Key::P) {
          pause();
        }
      }
      break;
    case KbdState::ConfirmQuit:
      if (key == Key::Y) {
        quitting_ = true;
        kbdState_ = KbdState::Main;
      } else if (key == Key::N) {
        kbdState_ = KbdState::Main;
      }
      break;
    case KbdState::Paused:
      switch (key) {
        case Key::M: pauseOptionMusic(); break;
        case Key::R: pauseOptionResolution(); break;
        case Key::S: pauseOptionScrolling(); break;
        case Key::A: pauseOptionAngle(); break;
        case Key::P: unpause(); break;
        case Key::Escape: pauseConfirmQuit(); break;
        default: break;
      }
      break;
    case KbdState::PausedConfirmQuit:
      if (key == Key::Y) {
        dm_.pixels = dm_.saved;
        quitting_ = true;
        kbdState_ = KbdState::Main;
      } else {
        unpause();
      }
      break;
    case KbdState::GetName:
      if (chr && nameBuf_.size() < 3) nameBuf_.push_back(chr);
      break;
  }
}

void Table::render(u8* data, Rgb* pal) const {
  for (std::size_t i = 0; i < 256; ++i) pal[i] = assets_.palette[i];
  for (std::size_t l = 0; l < assets_.lights.size(); ++l) {
    const Light& light = assets_.lights[l];
    for (std::size_t i = 0; i < light.colors.size(); ++i) {
      const Rgb c = light.colors[i];
      pal[light.baseIndex + i] = lights_[l].lit ? c : Rgb{static_cast<u8>(c.r / 2), static_cast<u8>(c.g / 2), static_cast<u8>(c.b / 2)};
    }
  }
  pal[assets_.dmPalette.indexOn] = dm_.state ? assets_.dmPalette.colorOn : assets_.dmPalette.colorOff;
  const int height = screenHeight() == 576 + 33 ? 576 : screenHeight() - 33;
  const int springPos = springPos_ / 2;
  const auto [bx, by0] = ball_.pos();
  const int by = ball_.frozen ? by0 : by0 + push_.offset();
  for (int y = 0; y < height; ++y) {
    const int sy = y + scroll_.pos + push_.offset();
    u8* row = data + static_cast<std::size_t>(y) * 320;
    if (sy >= 576)
      std::fill(row, row + 320, 0);
    else
      for (int x = 0; x < 320; ++x) row[x] = assets_.mainBoard(x, sy);
    if (sy >= 556 && sy < 556 + 17) {
      const int springY = sy - 553;
      if (springY >= springPos)
        for (int sx = 0; sx < 10; ++sx) row[sx + 304] = assets_.spring(sx, springY - springPos);
    }
    for (std::size_t f = 0; f < assets_.flippers.size(); ++f) {
      const Flipper& fl = assets_.flippers[f];
      const Grid8& gfx = fl.gfx[flippers_[f].quantum];
      if (sy >= fl.rectY && sy - fl.rectY < gfx.height())
        for (int fx = 0; fx < gfx.width(); ++fx) row[fx + fl.rectX] = gfx(fx, sy - fl.rectY);
    }
    if (!inAttract_ && sy >= by && sy < by + 15) {
      const int ballY = sy - by;
      for (int ballX = 0; ballX < 15; ++ballX) {
        const u8 pix = assets_.ball(ballX, ballY);
        if (pix == 0) continue;
        const int x = ballX + bx;
        if (x < 0 || x >= 320) continue;
        if (sy < 576 && assets_.occmaps[static_cast<std::size_t>(ball_.layer)](x, sy) != 0) continue;
        row[x] = pix;
      }
    }
  }
  const int fullHeight = height + 33;
  for (int y = height; y < fullHeight; ++y) std::fill(data + static_cast<std::size_t>(y) * 320, data + static_cast<std::size_t>(y + 1) * 320, 0);
  for (int y = 0; y < 16; ++y) {
    u8* row = data + static_cast<std::size_t>(2 + 2 * y + height) * 320;
    for (int x = 0; x < 160; ++x)
      row[x * 2] = dm_.pixels[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ? assets_.dmPalette.indexOn
                                                                                          : assets_.dmPalette.indexOff;
  }
  if (options_.mono)
    for (std::size_t i = 0; i < 256; ++i) {
      const u8 m = static_cast<u8>((pal[i].r + pal[i].g + pal[i].b) / 3);
      pal[i] = {m, m, m};
    }
  if (fade_ != 0x100)
    for (std::size_t i = 0; i < 256; ++i)
      pal[i] = {static_cast<u8>(pal[i].r * fade_ >> 8), static_cast<u8>(pal[i].g * fade_ >> 8),
                static_cast<u8>(pal[i].b * fade_ >> 8)};
}

// ---- scrolling (scroll.rs) ---------------------------------------------------------------

void Table::Scroll::setResolution(Resolution r, std::optional<i16> ballY) {
  switch (r) {
    case Resolution::Normal: windowHeight = 240 - 33; ballTarget = 75; break;
    case Resolution::High: windowHeight = 350 - 33; ballTarget = 130; break;
    case Resolution::Full: windowHeight = 576; ballTarget = 0; break;
  }
  u16 p;
  if (targetSpecial)
    p = *targetSpecial;
  else if (ballY)
    p = *ballY < ballTarget ? 0 : static_cast<u16>(*ballY - ballTarget);
  else
    p = 0;
  pos = std::min<u16>(p, static_cast<u16>(576 - windowHeight));
  rawPosF4 = static_cast<i16>(pos << 4);
}

void Table::Scroll::update(i16 ballY) {
  if (windowHeight == 576) {
    pos = 0;
    return;
  }
  const u16 target = targetSpecial ? *targetSpecial
                     : ballY < ballTarget ? 0
                                          : std::min<u16>(static_cast<u16>(ballY - ballTarget), static_cast<u16>(576 - windowHeight));
  i16 delta = static_cast<i16>(target - (rawPosF4 >> 4));
  rawPosF4 = static_cast<i16>(rawPosF4 + ((delta * speed) >> 2));
  delta = static_cast<i16>(target - (rawPosF4 >> 4));
  if (delta <= -ballTarget)
    rawPosF4 = static_cast<i16>(rawPosF4 + ((delta + ballTarget) << 4));
  else if (delta >= ballTarget + 40)
    rawPosF4 = static_cast<i16>(rawPosF4 + ((delta - ballTarget - 40) << 4));
  pos = static_cast<u16>(rawPosF4 >> 4);
}

void Table::Scroll::attractFrame() {
  if (windowHeight == 576) {
    pos = 0;
    return;
  }
  if (pos == 0)
    attractUp = false;
  else if (pos == 576 - windowHeight)
    attractUp = true;
  pos = static_cast<u16>(attractUp ? pos - 1 : pos + 1);
  rawPosF4 = static_cast<i16>(pos << 4);
}

void Table::Scroll::setSpecialTargetNow(u16 target) {
  targetSpecial = target;
  if (windowHeight != 576) {
    rawPosF4 = static_cast<i16>(target << 4);
    pos = target;
  }
}

void Table::Push::frame(bool state) {
  if (state) {
    speed = speedAttack;
    offsetF9 = static_cast<i16>(offsetF9 + speed);
    if (offsetF9 > 0x800) {
      speed = 0;
      offsetF9 = 0x800;
    }
  } else {
    speed = speedRelease;
    offsetF9 = static_cast<i16>(offsetF9 + speed);
    if (offsetF9 < 0) {
      speed = 0;
      offsetF9 = 0;
    }
  }
}

void Table::DotMatrix::blinkFrame() {
  if (!blink) return;
  if (--blink->first == 0) {
    blink->first = blink->second;
    state = !state;
  }
}

// ---- lights (lights.rs) --------------------------------------------------------------------

void Table::lightsAttractFrame() {
  for (std::size_t i = 0; i < attractCtr_.size(); ++i) {
    u16& ctr = attractCtr_[i];
    ++ctr;
    const AttractLight& a = assets_.attractLights[i];
    if (ctr == a.ctrOff) {
      lights_[a.light].lit = false;
    } else if (ctr == a.ctrOn) {
      lights_[a.light].lit = true;
      ctr = a.ctrReset;
    }
  }
}

void Table::setLightState(u8 light, bool state) { lights_[light] = LightState{state, state}; }

void Table::lightsReset() {
  for (std::size_t i = 0; i < lights_.size(); ++i) setLightState(static_cast<u8>(i), false);
}

void Table::lightsTilt() {
  for (LightState& l : lights_) {
    l.lit = false;
    l.blinking = false;
  }
}

void Table::lightsBlinkFrame() {
  for (LightState& l : lights_) {
    if (!l.blinking) continue;
    if (l.ctr == 0 || l.ctr == l.ctrReset) {
      l.lit = true;
      l.ctr = 0;
    } else if (l.ctr == l.ctrOff) {
      l.lit = false;
    }
    ++l.ctr;
  }
}

void Table::lightBlink(LightBind bind, u8 idx, u8 halfPeriod, u8 phase) {
  LightState& l = lights_[assets_.lightsOf(bind)[idx]];
  l.blinking = true;
  l.ctr = phase;
  l.ctrOff = halfPeriod;
  l.ctrReset = static_cast<u8>(halfPeriod * 2);
}

void Table::lightSet(LightBind bind, u8 idx, bool state) { setLightState(assets_.lightsOf(bind)[idx], state); }

void Table::lightSetAll(LightBind bind, bool state) {
  for (u8 l : assets_.lightsOf(bind)) setLightState(l, state);
}

bool Table::lightState(LightBind bind, u8 idx) const { return lights_[assets_.lightsOf(bind)[idx]].state; }

bool Table::lightAllLit(LightBind bind) const {
  for (u8 l : assets_.lightsOf(bind))
    if (!lights_[l].state) return false;
  return true;
}

bool Table::lightAllUnlit(LightBind bind) const {
  for (u8 l : assets_.lightsOf(bind))
    if (lights_[l].state) return false;
  return true;
}

void Table::lightRotate(LightBind bind) {
  const auto& ls = assets_.lightsOf(bind);
  std::vector<bool> states;
  for (u8 l : ls) states.push_back(lights_[l].state);
  for (std::size_t i = 0; i < ls.size(); ++i) setLightState(ls[i], states[(i + 1) % ls.size()]);
}

u8 Table::lightSequence(LightBind bind) {
  const auto& ls = assets_.lightsOf(bind);
  for (std::size_t i = 0; i < ls.size(); ++i)
    if (!lights_[ls[i]].state) {
      setLightState(ls[i], true);
      return static_cast<u8>(i);
    }
  return static_cast<u8>(ls.size());
}

// ---- sound (sound.rs) -----------------------------------------------------------------------

void Table::playSfxBind(SfxBind bind, u8 volume) {
  if (const auto& s = assets_.sfx(bind)) player_->playSfx(*s, volume);
}

bool Table::playJingleBind(JingleBind bind) { return sequencer_->playJingle(jingle(bind), false, std::nullopt); }
bool Table::playJingleBindForce(JingleBind bind) { return sequencer_->playJingle(jingle(bind), true, std::nullopt); }
bool Table::playJingleBindSilence(JingleBind bind) {
  return sequencer_->playJingle(jingle(bind), false, jingle(JingleBind::Silence).position);
}
void Table::setMusicSilence() { sequencer_->setMusic(jingle(JingleBind::Silence).position); }
void Table::setMusicPlunger() {
  sequencer_->setMusic(jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Plunger).position);
}
void Table::setMusicMain() {
  sequencer_->setMusic(jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Main).position);
}
void Table::playJinglePlunger() {
  const Jingle& j = jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Plunger);
  sequencer_->playJingle(j, false, j.position);
}

// ---- cheats (cheat.rs) -------------------------------------------------------------------------

void Table::handleCheat(u8 chr) {
  cheatBuf_.push_back(static_cast<char>(chr));
  bool foundPrefix = false;
  for (const Cheat& c : assets_.cheats) {
    if (cheatBuf_ == c.keys) {
      cheatBuf_.clear();
      switch (c.effect) {
        case CheatEffect::None: break;
        case CheatEffect::Tilt: cheatNoTilt_ = true; break;
        case CheatEffect::Slowdown: cheatSlowdown_ = true; break;
        case CheatEffect::Balls: totalBalls_ = 5; break;
        case CheatEffect::Reset:
          cheatNoTilt_ = false;
          cheatSlowdown_ = false;
          totalBalls_ = 3;
          break;
        default: break;
      }
      startScriptRaw(c.script);
      enterAttract_ = true;
      return;
    }
    if (c.keys.starts_with(cheatBuf_)) foundPrefix = true;
  }
  if (!foundPrefix) cheatBuf_ = std::string(1, static_cast<char>(chr));
}

}  // namespace pfr
