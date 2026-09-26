#include "game/Skins.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <string>
#include <thread>
#include <system_error>

#include "core/File.h"
#include "core/Log.h"
#include "core/Zip.h"
#include "gfx/HdLayer.h"
#include "platform/Download.h"

namespace pfr {
namespace {

constexpr const char* kSkinUrl = "https://files.catbox.moe/iz462e.zip";

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

/// What a set has to hold to count as being there: every picture the renderer knows by
/// name, the ball, and a pair of flippers for each table. A table's extra picture for its
/// upper bat is not asked for -- the game does without it where it is missing.
bool complete(const std::filesystem::path& dir, std::string& missing) {
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return false;
  const auto there = [&](const std::string& name) {
    if (std::filesystem::exists(dir / (name + ".png"), ec)) return true;
    missing = name + ".png";
    return false;
  };
  for (std::size_t i = 1; i < static_cast<std::size_t>(HdPicture::Count); ++i)
    if (!there(hdPictureName(static_cast<HdPicture>(i)))) return false;
  if (!there("ball")) return false;
  for (int table = 1; table <= 4; ++table)
    for (const char* side : {"_left", "_right"})
      if (!there("flipper" + std::to_string(table) + side)) return false;
  return true;
}

/// The fetching happens on a thread of its own so that the caller can keep drawing.
std::optional<Bytes> fetchWhileWaiting(const std::string& url, std::string& error, const SkinWaitTick& tick) {
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

bool downloadSkinOnce(const std::filesystem::path& gameDir, const SkinAsk& ask, const SkinWaitTick& tick) {
  std::error_code ec;
  const std::filesystem::path into = gameDir / kSkinFolder;
  std::string missing;
  if (complete(into, missing)) return true;
  if (std::filesystem::exists(into, ec))
    log::info(into.string() + " is missing " + missing + "; offering the pictures again");
  if (!ask || !ask()) {
    log::info("the pictures were not wanted; leaving");
    return false;
  }

  log::info(std::string("fetching the pictures from ") + kSkinUrl);
  std::string error;
  const auto zip = fetchWhileWaiting(kSkinUrl, error, tick);
  if (!zip) {
    log::error("the pictures could not be fetched: " + error);
    return true;
  }
  log::info("fetched " + std::to_string(zip->size() / 1024 / 1024) + " MB; unpacking");

  const auto entries = readZip(*zip);
  if (entries.empty()) {
    log::error("the archive holds nothing this reader can unpack");
    return true;
  }
  // Into a folder of its own first, so a transfer that stops halfway is not mistaken next
  // time for a set that is already there.
  const std::filesystem::path partial = gameDir / (std::string(kSkinFolder) + ".part");
  std::filesystem::remove_all(partial, ec);
  int written = 0;
  for (const auto& entry : entries) {
    if (!safeName(entry.name)) continue;
    // The archive carries its own top folder; the files inside it are what is wanted.
    const std::size_t slash = entry.name.find('/');
    const std::string inner = slash == std::string::npos ? entry.name : entry.name.substr(slash + 1);
    if (inner.empty()) continue;
    const std::filesystem::path path = partial / inner;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!file::writeAll(path, entry.data)) {
      log::error("cannot write " + path.string());
      std::filesystem::remove_all(partial, ec);
      return true;
    }
    ++written;
  }
  std::filesystem::remove_all(into, ec);
  std::filesystem::rename(partial, into, ec);
  if (ec) {
    log::error("cannot put the pictures in " + into.string() + ": " + ec.message());
    std::filesystem::remove_all(partial, ec);
    return true;
  }
  log::info(std::to_string(written) + " pictures unpacked into " + into.string());
  return true;
}

}  // namespace pfr
