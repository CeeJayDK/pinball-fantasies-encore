#pragma once
// SHA-256 (FIPS 180-4), used to recognise the exact game-file versions the engine supports.
#include <string>

#include "core/Types.h"

namespace pfr {

/// Lower-case hex digest of `data`.
std::string sha256Hex(ByteView data);

}  // namespace pfr
