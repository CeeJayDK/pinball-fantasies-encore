#pragma once
#include <filesystem>
#include <optional>

#include "core/Types.h"

namespace pfr::file {

/// Reads a whole file. Returns nullopt when the file cannot be opened.
std::optional<Bytes> readAll(const std::filesystem::path& path);

/// Writes a whole file atomically (temp file + rename). Returns false on failure.
bool writeAll(const std::filesystem::path& path, ByteView data);

/// Case-insensitive lookup of `name` inside `dir` (the DOS files may be upper or lower case).
std::optional<std::filesystem::path> findCaseInsensitive(const std::filesystem::path& dir, std::string_view name);

}  // namespace pfr::file
