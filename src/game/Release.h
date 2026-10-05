#pragma once
// Whether there is a newer release than this one. The game does not update itself: it says so
// at start, with a few lines about what is new, and offers to open the release's page. The
// server answers for GitHub (GET /v1/release); a version answered either way is not offered
// again.
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace encore {

struct ReleaseInfo {
  std::string version;  ///< as 1.2.3
  std::string url;      ///< the release's page
  std::vector<std::string> lines;  ///< what is new, in the letters the game can show
};

/// The release this build is, as 1.2.3, or empty for a build of no release.
std::string_view thisRelease();

/// The server's answer, or nothing if the text is not one.
std::optional<ReleaseInfo> parseRelease(std::string_view text);

/// The newest release. It blocks, so it is asked for from a thread of its own.
std::optional<ReleaseInfo> fetchLatestRelease(std::string* error = nullptr);

/// Whether a is a later version than b, both as 1.2.3.
bool newerVersion(std::string_view a, std::string_view b);

/// The newest version already offered, so that it is not offered again; empty for none.
std::string offeredRelease(const std::filesystem::path& saveDir);
void rememberOffered(const std::filesystem::path& saveDir, const std::string& version);

}  // namespace encore
