#pragma once
// Fetching a file over HTTP, and sending one, with whatever the platform already has: libcurl
// on macOS and Linux, WinHTTP on Windows. Nothing is installed for this.
#include <optional>
#include <string>
#include <vector>

#include "core/Types.h"

namespace pfr {

/// The whole body of a GET, or nothing on any failure, with why in error when it is given.
std::optional<Bytes> httpDownload(const std::string& url, std::string* error = nullptr);

/// What a server answered.
struct HttpReply {
  long status = 0;
  Bytes body;
};
/// POSTs `body` with the given header lines ("Name: value"). The server's answer whatever its
/// status, or nothing if none came, with why in error when it is given.
std::optional<HttpReply> httpPost(const std::string& url, ByteView body, const std::vector<std::string>& headers,
                                  std::string* error = nullptr);

}  // namespace pfr
