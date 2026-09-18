#include "core/Log.h"

#include <cstdio>
#include <mutex>

namespace pfr::log {
namespace {
Level g_minimum = Level::Info;
std::mutex g_mutex;
const char* prefix(Level level) {
  switch (level) {
    case Level::Debug: return "debug";
    case Level::Info: return "info ";
    case Level::Warn: return "warn ";
    case Level::Error: return "error";
  }
  return "?";
}
}  // namespace

void setMinimumLevel(Level level) { g_minimum = level; }

void write(Level level, std::string_view message) {
  if (level < g_minimum) return;
  std::scoped_lock lock(g_mutex);
  std::fprintf(stderr, "[%s] %.*s\n", prefix(level), static_cast<int>(message.size()), message.data());
}

void debug(std::string_view m) { write(Level::Debug, m); }
void info(std::string_view m) { write(Level::Info, m); }
void warn(std::string_view m) { write(Level::Warn, m); }
void error(std::string_view m) { write(Level::Error, m); }

}  // namespace pfr::log
