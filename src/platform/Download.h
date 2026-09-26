#pragma once
// Fetching a file over HTTP, with whatever the platform already has: libcurl on macOS and
// Linux, WinHTTP on Windows. Nothing is installed for this.
#include <optional>
#include <string>

#include "core/Types.h"

namespace pfr {

/// The whole body of a GET, or nothing on any failure, with why in error when it is given.
std::optional<Bytes> httpDownload(const std::string& url, std::string* error = nullptr);

}  // namespace pfr
