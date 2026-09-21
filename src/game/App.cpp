#include "game/App.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <optional>
#include <chrono>

#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/Png.h"
#include "platform/DataLocator.h"
#include "platform/ImageFile.h"

namespace pfr {
namespace {

constexpr double kFrame = 1.0 / 60.0;  ///< both screens run 60 frames a second, as in pfr

/// The table's 240- and 350-line screens fill a 4:3 display, so their pixels are not
/// square. The full-height mode keeps the 350-line pixel shape and shows the whole table.
double tablePixelAspect(int height) {
  const int shaped = height > 350 ? 350 : height;
  return (4.0 / 3.0) / (320.0 / shaped);
}

std::filesystem::path executableDir() {
  const char* base = SDL_GetBasePath();
  return base ? std::filesystem::path(base) : std::filesystem::current_path();
}

/// The keys the game understands, from SDL key codes (the original layout: Shift, Ctrl or
/// Alt for the flippers, Space to nudge, Down to pull the plunger, F1-F4 for the tables).
Key keyFor(SDL_Keycode k) {
  switch (k) {
    case SDLK_LSHIFT: return Key::ShiftLeft;
    case SDLK_RSHIFT: return Key::ShiftRight;
    case SDLK_LCTRL: return Key::ControlLeft;
    case SDLK_RCTRL: return Key::ControlRight;
    case SDLK_LALT: return Key::AltLeft;
    case SDLK_RALT: return Key::AltRight;
    case SDLK_SPACE: return Key::Space;
    case SDLK_DOWN: return Key::ArrowDown;
    case SDLK_UP: return Key::ArrowUp;
    case SDLK_LEFT: return Key::ArrowLeft;
    case SDLK_RIGHT: return Key::ArrowRight;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return Key::Enter;
    case SDLK_ESCAPE: return Key::Escape;
    default: break;
  }
  if (k >= SDLK_F1 && k <= SDLK_F8) return static_cast<Key>(static_cast<int>(Key::F1) + static_cast<int>(k - SDLK_F1));
  if (k >= SDLK_1 && k <= SDLK_8) return static_cast<Key>(static_cast<int>(Key::Digit1) + static_cast<int>(k - SDLK_1));
  if (k >= SDLK_A && k <= SDLK_Z) return static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(k - SDLK_A));
  return Key::None;
}

}  // namespace

App::App(AppOptions options) : options_(std::move(options)), frame_(640, 480) {}

bool App::init() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    log::error(std::string("SDL_Init: ") + SDL_GetError());
    return false;
  }
  auto dataDir = locateGameData(options_.dataDir);
  if (!dataDir) dataDir = askForGameData();
  if (!dataDir) {
    log::error("the Pinball Fantasies data files were not found");
    reportMissingGameData();
    return false;
  }
  files_ = GameFiles::fromDirectory(*dataDir);
  rememberGameData(*dataDir);
  log::info("game data: " + files_.directory.string());
  // Options and high scores live beside the app's preferences, never in the game folder.
  saveDir_ = preferencesDir();
  config_ = Config::load(saveDir_, files_.directory);
  if (options_.resolution) config_.options.resolution = *options_.resolution;

  // Prefer the shaders in the source tree while developing, so edits take effect at once.
  const std::filesystem::path source = std::filesystem::path(PFR_SOURCE_DIR) / "shaders";
  shaderDir_ = std::filesystem::exists(source) ? source : executableDir() / "shaders";

  if (!window_.create("Pinball Fantasies", 640 * std::max(1, options_.windowScale) / 2,
                      480 * std::max(1, options_.windowScale) / 2))
    return false;
  if (options_.fullscreen) window_.setFullscreen(true);
  if (!renderer_.init(shaderDir_, 640, 480, 1.0)) return false;
  renderer_.setSmoothEdges(options_.smoothEdges);
  {
    const auto saved = file::readAll(saveDir_ / "crt.txt");
    setCrt(options_.crt.value_or(saved && !saved->empty() && (*saved)[0] == '1'));
  }
  loadHdPictures();
  {
    const auto saved = file::readAll(saveDir_ / "hd.txt");
    renderer_.setHdEnabled(options_.hd.value_or(!saved || saved->empty() || (*saved)[0] != '0'));
    if (options_.hd) setHd(*options_.hd);
  }
  audio_.open(48000);

  if (options_.table >= 1 && options_.table <= 4)
    openTable(options_.table - 1);
  else
    openIntro(options_.skipIntro ? 0 : -1);
  return true;
}

/// The CRT look on or off, remembered for next time.
void App::setCrt(bool on) {
  renderer_.setCrt(on);
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "crt.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("CRT look ") + (on ? "on" : "off"));
}

/// High-resolution replacements for the intro's pictures, named after HdPicture
/// (slide1.png ... slide5.png, left.png, table1.png ... table4.png, hiscores.png). Each file
/// must show the whole original picture, edge to edge, at any size. The application carries
/// its own (assets/hd); one in --hd-dir, or else in hd/ in the preferences folder, takes its place.
/// Where a replacement picture comes from: the folder given with --hd-dir or hd/ in the
/// preferences folder first, then the application's own.
std::filesystem::path App::hdPicturePath(const std::string& name) const {
  const std::filesystem::path own = options_.hdDir.value_or(saveDir_ / "hd");
  if (std::filesystem::exists(own / name)) return own / name;
  const std::filesystem::path source = std::filesystem::path(PFR_SOURCE_DIR) / "assets" / "hd";
  return (std::filesystem::exists(source) ? source : executableDir() / "hd") / name;
}

void App::loadHdPictures() {
  int count = 0;
  for (std::size_t i = 1; i < HdFrame::kCount; ++i) {
    const auto p = static_cast<HdPicture>(i);
    const auto path = hdPicturePath(std::string(hdPictureName(p)) + ".png");
    if (!std::filesystem::exists(path)) continue;
    const auto image = loadImageFile(path);
    if (!image) {
      log::error("cannot read " + path.string());
      continue;
    }
    renderer_.setHdPicture(p, image->width, image->height, image->pixels.data());
    ++count;
  }
  if (count) log::info("replacement pictures: " + std::to_string(count));
}

/// The flippers drawn at any angle: a picture of each on its own, if there is one, and
/// otherwise the flipper cut out of the original artwork.
void App::loadFlipperPictures(int table) {
  renderer_.clearSpritePictures();
  const auto cutOut = table_->flipperPictures();
  const auto sides = table_->flipperSides();
  ownFlipperPictures_ = 0;
  std::array<int, 2> seen{};
  int own = 0;
  for (std::size_t f = 0; f < cutOut.size(); ++f) {
    const bool left = sides[f] == FlipperSide::Left;
    const int nth = ++seen[left ? 0 : 1];
    const std::string name = "flipper" + std::to_string(table + 1) + (left ? "_left" : "_right") +
                             (nth > 1 ? std::to_string(nth) : "") + ".png";
    const auto path = hdPicturePath(name);
    std::optional<RgbaImage> picture;
    if (std::filesystem::exists(path)) {
      picture = loadImageFile(path);
      if (!picture) log::error("cannot read " + path.string());
    }
    if (picture) {
      renderer_.setSpritePicture(f, picture->width, picture->height, picture->pixels.data());
      ownFlipperPictures_ = static_cast<u8>(ownFlipperPictures_ | (1u << f));
      ++own;
    } else {
      renderer_.setSpritePicture(f, cutOut[f].width, cutOut[f].height, cutOut[f].rgba.data());
    }
  }
  if (own) log::info("flipper pictures: " + std::to_string(own) + " of " + std::to_string(cutOut.size()));

  // The ball: its own picture if there is one, and otherwise the original's.
  std::optional<RgbaImage> ball;
  for (const std::string& name : {"ball" + std::to_string(table + 1) + ".png", std::string("ball.png")}) {
    const auto path = hdPicturePath(name);
    if (!std::filesystem::exists(path)) continue;
    ball = loadImageFile(path);
    if (!ball) log::error("cannot read " + path.string());
    break;
  }
  if (ball) {
    renderer_.setSpritePicture(HdSprite::kBall, ball->width, ball->height, ball->pixels.data());
    log::info("ball picture: " + std::to_string(ball->width) + "x" + std::to_string(ball->height));
  } else {
    const auto original = table_->ballPicture();
    renderer_.setSpritePicture(HdSprite::kBall, original.width, original.height, original.rgba.data());
  }
}

/// The replacement pictures on or off, remembered for next time.
void App::setHd(bool on) {
  renderer_.setHdEnabled(on);
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "hd.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("replacement pictures ") + (on ? "on" : "off"));
}

void App::resizeFrame(int width, int height, double pixelAspect) {
  if (frame_.width() != width || frame_.height() != height) frame_ = Framebuffer(width, height);
  renderer_.setPixelAspect(options_.squarePixels ? 1.0 : pixelAspect);
}

void App::openIntro(int returningFrom) {
  audio_.setSource({});
  table_.reset();
  // The slideshow plays to INTRO.MOD; coming back from a table the menu plays MOD2.MOD.
  const auto prg = file::readAll(files_.intro);
  const auto mod = file::readAll(returningFrom < 0 ? files_.introMusic : files_.menuMusic);
  if (!prg || !mod) throw DataError("cannot read INTRO.PRG or its music");
  intro_ = std::make_unique<Intro>(*prg, *mod, config_, returningFrom);
  resizeFrame(intro_->width(), intro_->height(), 1.0);
  audio_.setSource([p = &intro_->player()](float* out, int frames) { p->render(out, frames); });
}

void App::openTable(int index) {
  audio_.setSource({});
  intro_.reset();
  const auto prg = file::readAll(files_.tables[static_cast<std::size_t>(index)]);
  const auto mod = file::readAll(files_.tableMusic[static_cast<std::size_t>(index)]);
  if (!prg || !mod) throw DataError("cannot read the table files");
  const u64 seed = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
  table_ = std::make_unique<Table>(*prg, *mod, config_, index, seed);
  resizeFrame(320, table_->screenHeight(), tablePixelAspect(table_->screenHeight()));
  audio_.setSource([p = &table_->player()](float* out, int frames) { p->render(out, frames); });
  loadFlipperPictures(index);
  log::info("opened table " + std::to_string(index + 1));
}

void App::handleKey(const SDL_Event& e) {
  if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP) return;
  if (e.key.repeat) return;
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_GUI)) {
    window_.setFullscreen(!window_.fullscreen());
    return;
  }
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F9) {
    setCrt(!renderer_.crt());
    return;
  }
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F10) {
    if (renderer_.hasHdPictures()) setHd(!renderer_.hdEnabled());
    return;
  }
  const Key k = keyFor(e.key.key);
  if (k == Key::None) return;
  const bool down = e.type == SDL_EVENT_KEY_DOWN;
  if (table_) table_->handleKey(k, down);
  else if (intro_) intro_->handleKey(k, down);
}

void App::update(double dt) {
  clock_ += dt;
  while (clock_ >= kFrame) {
    clock_ -= kFrame;
    if (intro_) {
      const IntroAction a = intro_->runFrame();
      switch (a.kind) {
        case IntroAction::Kind::OpenTable:
          config_.options = intro_->options();
          openTable(a.table);
          break;
        case IntroAction::Kind::SaveOptions:
          config_.options = intro_->options();
          Config::saveOptions(saveDir_, config_.options);
          resizeFrame(intro_->width(), intro_->height(), 1.0);
          break;
        case IntroAction::Kind::Quit: running_ = false; return;
        case IntroAction::Kind::None: break;
      }
    } else if (table_) {
      const TableAction a = table_->runFrame();
      const int index = table_->tableIndex();
      switch (a.kind) {
        case TableAction::Kind::SaveOptions:
          config_.options = table_->options();
          Config::saveOptions(saveDir_, config_.options);
          break;
        case TableAction::Kind::SaveHighScores:
          config_.highScores[static_cast<std::size_t>(index)] = table_->highScores();
          Config::saveHighScores(saveDir_, index, table_->highScores());
          break;
        case TableAction::Kind::Quit:
          config_.options = table_->options();
          openIntro(index);
          return;
        case TableAction::Kind::None: break;
      }
      if (table_) resizeFrame(320, table_->screenHeight(), tablePixelAspect(table_->screenHeight()));
    }
  }
}

void App::render(double now) {
  std::array<Rgb, 256> colors{};
  // Only when the replacements will really be drawn: the table leaves the flippers out of the
  // frame for the renderer to put back, so with them off it must draw everything itself.
  HdFrame* const hd = renderer_.hasHdPictures() && renderer_.hdEnabled() ? &hd_ : nullptr;
  hd_.ownSprites = ownFlipperPictures_;
  if (table_)
    table_->render(frame_.data(), colors.data(), hd);
  else if (intro_)
    intro_->render(frame_.data(), colors.data(), hd);
  palette_.set(0, std::vector<Rgb>(colors.begin(), colors.end()));
  int w = 0, h = 0;
  window_.drawableSize(w, h);
  renderer_.setPalette(palette_);
  const auto beforeDraw = std::chrono::steady_clock::now();
  renderer_.draw(frame_, w, h, now, &hd_);
  if (options_.screenshot && ++frameCounter_ >= options_.screenshotFrame) {
    std::vector<u8> rgb(static_cast<std::size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    writeRgbPng(*options_.screenshot, rgb.data(), w, h, true);
    log::info("screenshot written to " + options_.screenshot->string());
    running_ = false;
  }
  const auto beforeSwap = std::chrono::steady_clock::now();
  window_.swap();
  if (options_.stats) {
    using ms = std::chrono::duration<double, std::milli>;
    stats_.draw += ms(beforeSwap - beforeDraw).count();
    stats_.wait += ms(std::chrono::steady_clock::now() - beforeSwap).count();
  }
}

int App::run() {
  try {
    if (!init()) return 1;
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    auto last = start;
    double reloadTimer = 0;
    while (running_) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running_ = false;
        handleKey(e);
      }
      const auto nowT = clock::now();
      const double dt = std::min(0.1, std::chrono::duration<double>(nowT - last).count());
      last = nowT;
      const auto beforeUpdate = clock::now();
      update(dt);
      if (options_.stats) {
        using ms = std::chrono::duration<double, std::milli>;
        stats_.update += ms(clock::now() - beforeUpdate).count();
        stats_.worst = std::max(stats_.worst, dt * 1000);
        stats_.seconds += dt;
        if (++stats_.frames >= 60 && stats_.seconds > 0) {
          const double n = stats_.frames;
          log::info("stats: " + std::to_string(n / stats_.seconds).substr(0, 5) + " frames a second, update " +
                    std::to_string(stats_.update / n).substr(0, 4) + " ms, draw " +
                    std::to_string(stats_.draw / n).substr(0, 4) + " ms, waiting for the screen " +
                    std::to_string(stats_.wait / n).substr(0, 4) + " ms, longest frame " +
                    std::to_string(stats_.worst).substr(0, 5) + " ms");
          stats_ = {};
        }
      }
      reloadTimer += dt;
      if (reloadTimer > 1.0) {
        reloadTimer = 0;
        renderer_.pollShaderReload();
      }
      render(std::chrono::duration<double>(nowT - start).count());
    }
  } catch (const DataError& e) {
    log::error(e.what());
    audio_.close();
    SDL_Quit();
    return 1;
  }
  audio_.setSource({});
  audio_.close();
  SDL_Quit();
  return 0;
}

}  // namespace pfr
