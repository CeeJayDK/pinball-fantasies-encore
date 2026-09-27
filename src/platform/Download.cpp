#include "platform/Download.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <dlfcn.h>
#endif

namespace pfr {

#ifdef _WIN32
namespace {

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

/// Closes a WinHTTP handle whichever way the function leaves.
struct Handle {
  HINTERNET h = nullptr;
  ~Handle() { if (h) WinHttpCloseHandle(h); }
  operator HINTERNET() const { return h; }
};

}  // namespace

std::optional<Bytes> httpDownload(const std::string& url, std::string* error) {
  const auto fail = [error](const char* what) -> std::optional<Bytes> {
    if (error) *error = what;
    return std::nullopt;
  };
  const std::wstring wide = widen(url);
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) return fail("the address cannot be read");

  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

  Handle session{WinHttpOpen(L"Pinball Fantasies: Encore!", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
  if (!session) return fail("no connection could be opened");
  Handle connection{WinHttpConnect(session, host.c_str(), parts.nPort, 0)};
  if (!connection) return fail("the server could not be reached");
  const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0u;
  Handle request{WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, flags)};
  if (!request) return fail("the request could not be made");
  if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request, nullptr))
    return fail("the server did not answer");

  DWORD status = 0, size = sizeof(status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status,
                      &size, WINHTTP_NO_HEADER_INDEX);
  if (status != 200) return fail("the server refused the file");

  Bytes body;
  for (;;) {
    DWORD waiting = 0;
    if (!WinHttpQueryDataAvailable(request, &waiting)) return fail("the transfer broke off");
    if (waiting == 0) break;
    const std::size_t at = body.size();
    body.resize(at + waiting);
    DWORD read = 0;
    if (!WinHttpReadData(request, body.data() + at, waiting, &read)) return fail("the transfer broke off");
    body.resize(at + read);
  }
  return body;
}

#else

// libcurl is asked of the system at the moment it is wanted, rather than linked: a machine
// without it then starts the game and is told what is missing, instead of being stopped by
// the loader before anything of this program runs. It is also the system's own copy, with
// the system's certificates, which is what a download wants. Only these six functions are
// used, and their shapes are declared here so that no header is needed to build.
namespace {

using CURL = void;
constexpr long kOptUrl = 10002, kOptWriteFunction = 20011, kOptWriteData = 10001;
constexpr long kOptErrorBuffer = 10010, kOptFollow = 52, kOptFailOnError = 45, kOptUserAgent = 10018;
constexpr std::size_t kErrorSize = 256;

struct Curl {
  void* lib = nullptr;
  CURL* (*easy_init)() = nullptr;
  int (*easy_setopt)(CURL*, long, ...) = nullptr;
  int (*easy_perform)(CURL*) = nullptr;
  void (*easy_cleanup)(CURL*) = nullptr;
  const char* (*easy_strerror)(int) = nullptr;

  /// The names a system may have for it, newest first; the versionless one is a dev package.
  static Curl& instance() {
    static Curl c = [] {
      Curl out;
      for (const char* name : {
#ifdef __APPLE__
             "libcurl.4.dylib", "libcurl.dylib",
#else
             "libcurl.so.4", "libcurl.so.3", "libcurl.so",
#endif
           }) {
        out.lib = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
        if (out.lib) break;
      }
      if (!out.lib) return out;
      const auto sym = [&](const char* name) { return dlsym(out.lib, name); };
      out.easy_init = reinterpret_cast<CURL* (*)()>(sym("curl_easy_init"));
      out.easy_setopt = reinterpret_cast<int (*)(CURL*, long, ...)>(sym("curl_easy_setopt"));
      out.easy_perform = reinterpret_cast<int (*)(CURL*)>(sym("curl_easy_perform"));
      out.easy_cleanup = reinterpret_cast<void (*)(CURL*)>(sym("curl_easy_cleanup"));
      out.easy_strerror = reinterpret_cast<const char* (*)(int)>(sym("curl_easy_strerror"));
      if (!out.easy_init || !out.easy_setopt || !out.easy_perform || !out.easy_cleanup) {
        dlclose(out.lib);
        out.lib = nullptr;
      }
      return out;
    }();
    return c;
  }
};

std::size_t collect(char* data, std::size_t size, std::size_t count, void* userdata) {
  auto* out = static_cast<Bytes*>(userdata);
  out->insert(out->end(), data, data + size * count);
  return size * count;
}

}  // namespace

std::optional<Bytes> httpDownload(const std::string& url, std::string* error) {
  const auto fail = [error](const std::string& what) -> std::optional<Bytes> {
    if (error) *error = what;
    return std::nullopt;
  };
  const Curl& curl = Curl::instance();
  if (!curl.lib)
    return fail("this computer has no libcurl, which is what downloading needs"
#ifndef __APPLE__
                " (it is the libcurl4 package on Debian and Ubuntu, libcurl on Arch,"
                " libcurl-minimal on Fedora)"
#endif
    );

  CURL* handle = curl.easy_init();
  if (!handle) return fail("no connection could be opened");
  Bytes body;
  char message[kErrorSize] = {};
  curl.easy_setopt(handle, kOptUrl, url.c_str());
  curl.easy_setopt(handle, kOptFollow, 1L);
  curl.easy_setopt(handle, kOptFailOnError, 1L);
  curl.easy_setopt(handle, kOptUserAgent, "Pinball Fantasies: Encore!");
  curl.easy_setopt(handle, kOptWriteFunction, collect);
  curl.easy_setopt(handle, kOptWriteData, &body);
  curl.easy_setopt(handle, kOptErrorBuffer, message);
  const int result = curl.easy_perform(handle);
  curl.easy_cleanup(handle);
  if (result != 0)
    return fail(message[0] ? message
                           : (curl.easy_strerror ? curl.easy_strerror(result) : "the download failed"));
  return body;
}

#endif

}  // namespace pfr
