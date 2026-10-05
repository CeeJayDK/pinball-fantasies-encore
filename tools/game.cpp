// encore-game: a table of the engine of our own played with no window, by keys pressed at
// random, to see whole games through: what each ended with, whether the same keys give the
// same game twice, and what it looks and sounds like.
//
//   encore-game <the game's folder> <table 1-4> <frames> <seed> [out.png] [out.wav]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/File.h"
#include "core/Png.h"
#include "engine/game/TableGame.h"

namespace {

using namespace encore;

std::string text(const Bcd& s) {
  std::string out;
  for (u8 d : s.digits)
    if (d != 0 || !out.empty()) out += static_cast<char>('0' + d % 10);
  return out.empty() ? "0" : out;
}

struct Outcome {
  u64 hash = 1469598103934665603ull;
  std::vector<Recording::Game> ended;
  Bytes recording;
  std::string failure;
};

Outcome play(ByteView prg, ByteView module, int table, int frames, unsigned seed, bool say,
             const char* png, std::vector<float>* sound) {
  TableGame::Setup setup;
  setup.options.balls = 3;
  setup.options.resolution = Resolution::High;
  setup.chance = static_cast<u16>(seed * 40503u);
  TableGame game(prg, module, table, setup);  // (best scores of nought: every game asks for initials)
  unsigned rng = seed * 2654435761u + 1;
  auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
  bool left = false, right = false, pulled = false;
  std::vector<float> chunk(800 * 2);
  Outcome out;
  std::size_t said = 0;
  for (int f = 0; f < frames && !game.left(); ++f) {
    auto tap = [&](Key k) { game.key(k, true), game.key(k, false); };
    if (game.paused()) {  // the options this version has there, and back to the game
      static constexpr Key kTry[] = {Key::A, Key::S, Key::R, Key::M, Key::F7, Key::ArrowUp, Key::Z, Key::M};
      if (random() % 20 == 0) tap(kTry[random() % 8]);
      if (random() % 120 == 0) tap(Key::P);
    } else if (game.waiting()) {
      if (f % 240 == 30) tap(random() % 4 == 0 ? Key::F2 : Key::Enter);
    } else {
      if (random() % 23 == 0) game.key(Key::ShiftLeft, left = !left);
      if (random() % 23 == 0) game.key(Key::ShiftRight, right = !right);
      if (!pulled && f % 300 == 100) game.key(Key::ArrowDown, pulled = true);
      else if (pulled && random() % 40 == 0) game.key(Key::ArrowDown, pulled = false);
      if (random() % 400 == 0) tap(Key::Space);
      if (random() % 2500 == 0) tap(Key::P);
      if (random() % 90 == 0) tap(static_cast<Key>(static_cast<int>(Key::A) + random() % 26));  // initials, when asked
    }
    game.frame();
    if (sound) {
      game.sound(chunk.data(), 800);
      sound->insert(sound->end(), chunk.begin(), chunk.end());
    } else {
      game.noSound();
    }
    for (u8 b : game.engine().memory()) out.hash = (out.hash ^ b) * 1099511628211ull;
    if (game.askingOnline() && random() % 30 == 0) tap(random() % 2 ? Key::Y : Key::N);
    for (; say && said < game.recording().games.size(); ++said) {
      const auto& e = game.recording().games[said];
      std::string line = "frame " + std::to_string(e.endFrame) + ": a game over" + (e.abandoned ? " (given up)" : "") + ":";
      for (const auto& s : e.scores) line += " " + text(s);
      if (e.initials[0]) line += std::string("  initials ") + static_cast<char>(e.initials[0]) + static_cast<char>(e.initials[1]) + static_cast<char>(e.initials[2]);
      std::puts(line.c_str());
    }
  }
  if (png) {
    std::vector<u8> pixels(320 * static_cast<std::size_t>(game.screenHeight()));
    std::vector<Rgb> colours(256);
    game.draw(pixels.data(), colours.data());
    writeIndexedPng(png, pixels.data(), 320, game.screenHeight(), colours);
  }
  if (say)
    std::printf("after %u frames: %s, player %d of %d, ball %d, score %s\n", game.frames(), game.waiting() ? "waiting" : "playing",
                game.player(), game.players(), game.ball(), text(game.score(0)).c_str());
  out.ended = game.recording().games;
  out.recording = game.recording().save();
  out.failure = game.failure();
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    std::puts("usage: encore-game <the game's folder> <table 1-4> <frames> <seed> [out.png] [out.wav]");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const int table = std::atoi(argv[2]) - 1;
  const int frames = std::atoi(argv[3]);
  const unsigned seed = static_cast<unsigned>(std::atoi(argv[4]));
  const auto prg = file::findCaseInsensitive(dir, "TABLE" + std::to_string(table + 1) + ".PRG");
  const auto modPath = file::findCaseInsensitive(dir, "TABLE" + std::to_string(table + 1) + ".MOD");
  const auto mod = modPath ? file::readAll(*modPath) : std::nullopt;
  const auto program = prg ? file::readAll(*prg) : std::nullopt;
  if (!program || !mod) {
    std::puts("cannot read the table's files");
    return 2;
  }
  std::vector<float> sound;
  const Outcome first = play(*program, *mod, table, frames, seed, true, argc > 5 ? argv[5] : nullptr, argc > 6 ? &sound : nullptr);
  const Outcome again = play(*program, *mod, table, frames, seed, false, nullptr, nullptr);
  if (argc > 6) {
    std::vector<u8> wav;
    auto put = [&](u32 v, int n) {
      for (int i = 0; i < n; ++i) wav.push_back(static_cast<u8>(v >> (8 * i)));
    };
    const auto size = static_cast<u32>(sound.size() * 2);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    put(36 + size, 4);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put(16, 4); put(1, 2); put(2, 2); put(48000, 4); put(48000 * 4, 4); put(4, 2); put(16, 2);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    put(size, 4);
    for (float s : sound) put(static_cast<u16>(static_cast<i16>((s < -1 ? -1 : s > 1 ? 1 : s) * 32767)), 2);
    file::writeAll(argv[6], ByteView(wav.data(), wav.size()));
  }
  if (!first.failure.empty()) std::printf("the table stopped: %s\n", first.failure.c_str());
  bool same = first.hash == again.hash && first.ended.size() == again.ended.size();
  // and as a recording of it, saved, read back and played by its keys alone
  const auto recording = Recording::load(first.recording);
  if (!recording) {
    std::puts("the recording of it cannot be read back");
    return 1;
  }
  const Recording replayed = replay(*program, *mod, *recording);
  const bool faithful = replayed.games == recording->games && replayed.events == recording->events && replayed.frames == recording->frames;
  std::printf("its recording (%zu bytes, %zu keys) played again: %s\n", first.recording.size(), recording->events.size(), faithful ? "the same" : "DIFFERENT");
  same = same && faithful;
  std::printf("%zu games; played again with the same keys: %s\n", first.ended.size(), same ? "the same" : "DIFFERENT");
  return same && first.failure.empty() ? 0 : 1;
}
