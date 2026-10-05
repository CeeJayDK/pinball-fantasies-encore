#include "platform/DataLocator.h"

#include <SDL3/SDL.h>
#include <optional>
#include <string>
#include <system_error>

#include "core/File.h"
#include "core/Log.h"
#include "data/GameVersion.h"

namespace encore {
namespace {

bool holdsGameFiles(const std::filesystem::path& dir) {
  return file::findCaseInsensitive(dir, "INTRO.PRG").has_value() &&
         file::findCaseInsensitive(dir, "TABLE1.PRG").has_value();
}

/// The files are there and are the version the engine supports.
bool holdsGame(const std::filesystem::path& dir) {
  if (!holdsGameFiles(dir)) return false;
  const auto bad = unsupportedGameFiles(dir);
  if (!bad.empty()) log::info(dir.string() + " holds a different release of the game; skipping it");
  return bad.empty();
}

}  // namespace

std::filesystem::path gameDataDir() {
  const std::filesystem::path prefs = preferencesDir();
  return prefs.empty() ? prefs : prefs / "FANTASY";
}

std::optional<std::filesystem::path> locateGameData(const std::optional<std::filesystem::path>& explicitDir) {
  // One place, known in advance: no folder is searched, nothing around the application is
  // stepped through, and nothing outside this version's own folder is read.
  if (explicitDir) return holdsGame(*explicitDir) ? std::optional(*explicitDir) : std::nullopt;
  const std::filesystem::path dir = gameDataDir();
  if (!dir.empty() && holdsGame(dir)) return dir;
  return std::nullopt;
}

std::filesystem::path preferencesDir() {
  char* base = SDL_GetPrefPath("Encore", "Pinball Fantasies");
  if (!base) return {};
  std::filesystem::path path(base);
  SDL_free(base);
  // Settings and high scores kept under the old name are taken over once, the first time the
  // new folder is used. The old folder is left as it was.
  std::error_code ec;
  if (std::filesystem::is_empty(path, ec) && !ec) {
    char* old = SDL_GetPrefPath("pfr", "Pinball Fantasies");
    if (old) {
      const std::filesystem::path from(old);
      SDL_free(old);
      if (from != path && std::filesystem::is_directory(from, ec)) {
        std::filesystem::copy(from, path, std::filesystem::copy_options::recursive, ec);
        if (!ec) log::info("settings and high scores taken over from " + from.string());
      }
    }
  }
  return path;
}

void reportMissingGameData() {
  const std::string msg =
      "The original Pinball Fantasies files were not found.\n\n"
      "This game reads the artwork, music and tables from your own copy of the 1994\n"
      "MS-DOS release. Put INTRO.PRG, TABLE1..4.PRG and the MOD files in:\n\n" +
      gameDataDir().string() + "\n\nor start the game with:  --data /path/to/pinball";
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinball Fantasies", msg.c_str(), nullptr);
}

void reportError(const std::string& message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinball Fantasies: Encore!", message.c_str(), nullptr);
}

}  // namespace encore
