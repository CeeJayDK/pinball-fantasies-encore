#include "game/Release.h"

#include <array>
#include <charconv>

#include "core/File.h"
#include "core/Types.h"
#include "game/Online.h"
#include "platform/Http.h"

namespace encore {
namespace {

constexpr const char* kOfferedFile = "release-offered.txt";

/// 1.2.3 as its three numbers, or nothing if it is not one.
std::optional<std::array<int, 3>> versionNumbers(std::string_view v) {
  std::array<int, 3> n{};
  for (std::size_t i = 0; i < 3; ++i) {
    const std::size_t dot = i < 2 ? v.find('.') : v.size();
    if (dot == std::string_view::npos) return std::nullopt;
    const auto [end, ec] = std::from_chars(v.data(), v.data() + dot, n[i]);
    if (ec != std::errc() || end != v.data() + dot) return std::nullopt;
    v.remove_prefix(i < 2 ? dot + 1 : dot);
  }
  return n;
}

}  // namespace

std::string_view thisRelease() {
  static const std::string_view v = versionNumbers(ENCORE_VERSION) ? ENCORE_VERSION : "";
  return v;
}

std::optional<ReleaseInfo> parseRelease(std::string_view text) {
  ReleaseInfo r;
  std::size_t at = 0;
  while (at < text.size()) {
    const std::size_t newline = text.find('\n', at);
    std::string_view line = text.substr(at, newline == std::string_view::npos ? std::string_view::npos : newline - at);
    at = newline == std::string_view::npos ? text.size() : newline + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.starts_with("version ")) r.version = line.substr(8);
    else if (line.starts_with("url ")) r.url = line.substr(4);
    else if (line == "line" || line.starts_with("line ")) r.lines.emplace_back(line.size() > 5 ? line.substr(5) : "");
  }
  if (!versionNumbers(r.version) || !r.url.starts_with("https://")) return std::nullopt;
  return r;
}

std::optional<ReleaseInfo> fetchLatestRelease(std::string* error) {
  const auto body = httpDownload(onlineApi() + "/v1/release", error);
  if (!body) return std::nullopt;
  auto r = parseRelease(std::string(body->begin(), body->end()));
  if (!r && error) *error = "the server's answer is not a release";
  return r;
}

bool newerVersion(std::string_view a, std::string_view b) {
  const auto x = versionNumbers(a), y = versionNumbers(b);
  return x && y && *x > *y;
}

std::string offeredRelease(const std::filesystem::path& saveDir) {
  const auto saved = file::readAll(saveDir / kOfferedFile);
  return saved ? std::string(saved->begin(), saved->end()) : std::string();
}

void rememberOffered(const std::filesystem::path& saveDir, const std::string& version) {
  file::writeAll(saveDir / kOfferedFile, ByteView(reinterpret_cast<const u8*>(version.data()), version.size()));
}

}  // namespace encore
