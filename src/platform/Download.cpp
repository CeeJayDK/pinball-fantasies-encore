#include "platform/Download.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
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

namespace {

std::size_t collect(char* data, std::size_t size, std::size_t count, void* userdata) {
  auto* out = static_cast<Bytes*>(userdata);
  out->insert(out->end(), data, data + size * count);
  return size * count;
}

}  // namespace

std::optional<Bytes> httpDownload(const std::string& url, std::string* error) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    if (error) *error = "no connection could be opened";
    return std::nullopt;
  }
  Bytes body;
  char message[CURL_ERROR_SIZE] = {};
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Pinball Fantasies: Encore!");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, message);
  const CURLcode result = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  if (result != CURLE_OK) {
    if (error) *error = message[0] ? message : curl_easy_strerror(result);
    return std::nullopt;
  }
  return body;
}

#endif

}  // namespace pfr
