#include "core/File.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <system_error>

namespace encore::file {

std::optional<Bytes> readAll(const std::filesystem::path& path) {
  // Streams take the path as it is; stdio would need it narrowed, which loses names on Windows.
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  Bytes out;
  u8 buffer[1 << 16];
  while (in.read(reinterpret_cast<char*>(buffer), sizeof buffer) || in.gcount())
    out.insert(out.end(), buffer, buffer + in.gcount());
  if (in.bad()) return std::nullopt;
  return out;
}

bool writeAll(const std::filesystem::path& path, ByteView data) {
  std::filesystem::path temp = path;
  temp += ".tmp";
  std::error_code ec;
  {
    std::ofstream out(temp, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) {
      out.close();
      std::filesystem::remove(temp, ec);
      return false;
    }
  }
  std::filesystem::rename(temp, path, ec);
  if (ec) {
    std::error_code ignored;
    std::filesystem::remove(temp, ignored);
  }
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
  // Asked not to throw at every step, not only at the first: a folder that cannot be read all
  // the way through is answered with "not here" rather than an exception.
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (equalsIgnoreCase(it->path().filename().string(), name)) return it->path();
  return std::nullopt;
}

}  // namespace encore::file
