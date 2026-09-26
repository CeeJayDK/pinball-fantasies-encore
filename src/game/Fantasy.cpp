#include "game/Fantasy.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <string>
#include <thread>
#include <system_error>

#include "core/File.h"
#include "core/Log.h"
#include "core/Zip.h"
#include "platform/Download.h"

namespace pfr {
namespace {

constexpr const char* kFantasyUrl = "https://files.catbox.moe/iz462e.zip";

/// An archive says where each file goes, so it decides where the writing happens: anything
/// reaching outside the folder it is being unpacked into is dropped, as are the entries a
/// Mac adds to a zip of its own accord.
bool safeName(const std::string& name) {
  if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos) return false;
  if (name.size() > 1 && name[1] == ':') return false;
  std::size_t at = 0;
  while (at <= name.size()) {
    const std::size_t slash = name.find('/', at);
    const std::string part = name.substr(at, slash == std::string::npos ? std::string::npos : slash - at);
    if (part == ".." || part == "." || part.empty()) return false;
    if (part == "__MACOSX" || part.front() == '.') return false;
    if (slash == std::string::npos) break;
    at = slash + 1;
  }
  return true;
}

/// What a finished download leaves behind. It holds the address the archive came from, so
/// that a later archive at a different address is fetched even though this one is unpacked.
constexpr const char* kDoneFile = "downloaded.txt";

bool alreadyHere(const std::filesystem::path& gameDir) {
  const auto done = file::readAll(gameDir / kDoneFile);
  return done && std::string(done->begin(), done->end()).find(kFantasyUrl) != std::string::npos;
}

/// The fetching happens on a thread of its own so that the caller can keep drawing.
std::optional<Bytes> fetchWhileWaiting(const std::string& url, std::string& error, const FantasyWaitTick& tick) {
  std::optional<Bytes> body;
  std::atomic<bool> done{false};
  std::thread worker([&] {
    body = httpDownload(url, &error);
    done.store(true, std::memory_order_release);
  });
  const u64 start = SDL_GetTicks();
  while (!done.load(std::memory_order_acquire)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }
    if (tick) tick(static_cast<double>(SDL_GetTicks() - start) / 1000.0);
    SDL_Delay(16);
  }
  worker.join();
  return body;
}

}  // namespace

bool downloadFantasyOnce(const std::filesystem::path& gameDir, const FantasyAsk& ask, const FantasyWaitTick& tick) {
  std::error_code ec;
  if (alreadyHere(gameDir)) return true;
  if (!ask || !ask()) {
    log::info("the download was not wanted; leaving");
    return false;
  }

  log::info(std::string("fetching ") + kFantasyUrl);
  std::string error;
  const auto zip = fetchWhileWaiting(kFantasyUrl, error, tick);
  if (!zip) {
    log::error("the archive could not be fetched: " + error);
    return true;
  }
  log::info("fetched " + std::to_string(zip->size() / 1024 / 1024) + " MB; unpacking");

  const auto entries = readZip(*zip);
  if (entries.empty()) {
    log::error("the archive holds nothing this reader can unpack");
    return true;
  }
  // Whatever the archive holds, where it says to put it, under the game folder.
  std::filesystem::create_directories(gameDir, ec);
  int written = 0;
  for (const auto& entry : entries) {
    if (!safeName(entry.name)) continue;
    const std::filesystem::path path = gameDir / entry.name;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!file::writeAll(path, entry.data)) {
      log::error("cannot write " + path.string());
      return true;
    }
    ++written;
  }
  log::info(std::to_string(written) + " files unpacked into " + gameDir.string());
  // Only now, with everything written: a run that stops halfway asks again next time.
  const std::string done = std::string(kFantasyUrl) + "\n";
  file::writeAll(gameDir / kDoneFile, ByteView(reinterpret_cast<const u8*>(done.data()), done.size()));
  return true;
}

}  // namespace pfr
