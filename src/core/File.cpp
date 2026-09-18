#include "core/File.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>

namespace pfr::file {

std::optional<Bytes> readAll(const std::filesystem::path& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return std::nullopt;
  Bytes out;
  u8 buffer[1 << 16];
  for (;;) {
    const std::size_t n = std::fread(buffer, 1, sizeof buffer, f);
    out.insert(out.end(), buffer, buffer + n);
    if (n < sizeof buffer) break;
  }
  const bool ok = std::ferror(f) == 0;
  std::fclose(f);
  if (!ok) return std::nullopt;
  return out;
}

bool writeAll(const std::filesystem::path& path, ByteView data) {
  std::filesystem::path temp = path;
  temp += ".tmp";
  std::FILE* f = std::fopen(temp.c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
  std::fclose(f);
  if (!ok) {
    std::remove(temp.c_str());
    return false;
  }
  std::error_code ec;
  std::filesystem::rename(temp, path, ec);
  if (ec) std::remove(temp.c_str());
  return !ec;
}

std::optional<std::filesystem::path> findCaseInsensitive(const std::filesystem::path& dir, std::string_view name) {
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return std::nullopt;
  auto equalsIgnoreCase = [](std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
             return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
  };
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (equalsIgnoreCase(entry.path().filename().string(), name)) return entry.path();
  }
  return std::nullopt;
}

}  // namespace pfr::file
