#pragma once
#include <string_view>

namespace pfr::log {

enum class Level { Debug, Info, Warn, Error };

void setMinimumLevel(Level level);
void write(Level level, std::string_view message);

void debug(std::string_view message);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

}  // namespace pfr::log
