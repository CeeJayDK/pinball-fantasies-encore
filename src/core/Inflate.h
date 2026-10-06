#pragma once
// Deflate decoding (RFC 1951), and the zlib wrapper around it (RFC 1950): the PNG reader's
// own inflate, shared so that zip archives can be unpacked with it too.
#include "core/Types.h"

namespace encore {

/// A bare deflate stream, as a zip entry stores it. Appends to out.
bool inflateRaw(ByteView deflate, Bytes& out);

/// A deflate stream inside a zlib wrapper, as a PNG's IDAT holds it.
bool inflateZlib(ByteView zlib, Bytes& out);

}  // namespace encore
