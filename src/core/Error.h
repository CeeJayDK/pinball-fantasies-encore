#pragma once
#include <stdexcept>
#include <string>

namespace encore {

/// Thrown for unrecoverable data problems (missing or corrupt game files).
class DataError : public std::runtime_error {
 public:
  explicit DataError(const std::string& what) : std::runtime_error(what) {}
};

}  // namespace encore
