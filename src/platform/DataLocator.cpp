#include "platform/DataLocator.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <fstream>
#include <optional>

#include "core/File.h"
#include "core/Log.h"
#include "data/GameVersion.h"

namespace pfr {
namespace {

constexpr int kParentLevels = 6;

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

std::filesystem::path preferencesFile() {
  const std::filesystem::path dir = preferencesDir();
  return dir.empty() ? dir : dir / "data-folder.txt";
}

std::optional<std::filesystem::path> rememberedFolder() {
  const std::filesystem::path file = preferencesFile();
  if (file.empty()) return std::nullopt;
  std::ifstream in(file);
  std::string line;
  if (!std::getline(in, line) || line.empty()) return std::nullopt;
  return std::filesystem::path(line);
}

/// Adds a folder and its parents, so the app finds the game whether it sits beside the
/// files, inside a build folder under them, or anywhere in between.
void addWithParents(std::vector<std::filesystem::path>& out, std::filesystem::path dir) {
  std::error_code ec;
  dir = std::filesystem::weakly_canonical(dir, ec);
  for (int i = 0; i <= kParentLevels && !dir.empty(); ++i) {
    out.push_back(dir);
    // Sibling installs, such as a second copy of the game next to this one.
    for (const auto& child : std::filesystem::directory_iterator(dir, ec))
      if (child.is_directory(ec)) out.push_back(child.path());
    if (!dir.has_parent_path() || dir.parent_path() == dir) break;
    dir = dir.parent_path();
  }
}

struct FolderChoice {
  bool done = false;
  std::optional<std::filesystem::path> path;
};

void SDLCALL folderChosen(void* userdata, const char* const* files, int) {
  auto* choice = static_cast<FolderChoice*>(userdata);
  if (files && files[0]) choice->path = std::filesystem::path(files[0]);
  choice->done = true;
}

}  // namespace

std::optional<std::filesystem::path> locateGameData(const std::optional<std::filesystem::path>& explicitDir) {
  std::vector<std::filesystem::path> candidates;
  if (explicitDir) addWithParents(candidates, *explicitDir);
  if (auto remembered = rememberedFolder()) candidates.push_back(*remembered);
  std::error_code ec;
  addWithParents(candidates, std::filesystem::current_path(ec));
  if (const char* base = SDL_GetBasePath()) addWithParents(candidates, std::filesystem::path(base));

  for (const auto& dir : candidates) {
    if (dir.empty()) continue;
    if (holdsGame(dir)) return dir;
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> askForGameData() {
  FolderChoice choice;
  SDL_ShowOpenFolderDialog(folderChosen, &choice, nullptr, nullptr, false);
  // The chooser reports back through the event queue, so keep pumping until it answers.
  for (int waited = 0; !choice.done && waited < 120000; waited += 10) {
    SDL_PumpEvents();
    SDL_Delay(10);
  }
  if (!choice.path) return std::nullopt;
  if (holdsGameFiles(*choice.path) && !holdsGame(*choice.path)) {
    std::string names;
    for (const auto& n : unsupportedGameFiles(*choice.path)) names += "  " + n + "\n";
    const std::string msg =
        "That folder holds a different release of Pinball Fantasies. These files differ from the\n"
        "version this remake reads:\n\n" + names +
        "\nThe supported version is the original disk release, archived at\n"
        "https://archive.org/details/000323-PinballFantasies";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Pinball Fantasies", msg.c_str(), nullptr);
    return std::nullopt;
  }
  if (!holdsGame(*choice.path)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Pinball Fantasies",
                             "That folder does not contain the game files.\n\n"
                             "Choose the folder holding INTRO.PRG and TABLE1.PRG.",
                             nullptr);
    return std::nullopt;
  }
  return choice.path;
}

std::filesystem::path preferencesDir() {
  char* base = SDL_GetPrefPath("pfr", "Pinball Fantasies");
  if (!base) return {};
  std::filesystem::path path(base);
  SDL_free(base);
  return path;
}

void rememberGameData(const std::filesystem::path& dir) {
  const std::filesystem::path file = preferencesFile();
  if (file.empty()) return;
  std::ofstream out(file, std::ios::trunc);
  out << dir.string() << "\n";
}

void reportMissingGameData() {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Pinball Fantasies",
                           "The original Pinball Fantasies files were not found.\n\n"
                           "This game reads the artwork, music and tables from your own copy of the\n"
                           "1994 MS-DOS release. Put this application in the folder holding INTRO.PRG\n"
                           "and TABLE1.PRG, or start it with:  --data /path/to/pinball",
                           nullptr);
}

}  // namespace pfr
