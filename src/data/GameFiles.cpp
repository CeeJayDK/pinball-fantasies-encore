#include "data/GameFiles.h"

#include "core/Error.h"
#include "core/File.h"

namespace encore {

GameFiles GameFiles::fromDirectory(const std::filesystem::path& dir) {
  auto require = [&](std::string_view name) {
    auto p = file::findCaseInsensitive(dir, name);
    if (!p) throw DataError("missing game file " + std::string(name) + " in " + dir.string());
    return *p;
  };
  GameFiles g;
  g.directory = dir;
  g.intro = require("INTRO.PRG");
  g.introMusic = require("INTRO.MOD");
  g.menuMusic = require("MOD2.MOD");
  for (std::size_t i = 0; i < 4; ++i) {
    const std::string n = std::to_string(i + 1);
    g.tables[i] = require("TABLE" + n + ".PRG");
    g.tableMusic[i] = require("TABLE" + n + ".MOD");
  }
  return g;
}

}  // namespace encore
