#pragma once
// Reader for the 16-bit MZ executables the game ships as (INTRO.PRG, TABLEn.PRG).
//
// The game never runs this code; we only use the executables as containers. The
// relocation table tells us every segment value the code references, which yields
// an exact map of the data blocks (masks, sprite banks, image strips) inside.
#include <filesystem>

#include "core/Types.h"

namespace encore {

class MzImage {
 public:
  struct Segment {
    u16 value;         ///< Segment value as seen by the code (relative to the load base).
    std::size_t offset;  ///< Byte offset of the segment inside the load image.
    std::size_t length;  ///< Bytes up to the next referenced segment (or end of image).
  };

  static MzImage load(const std::filesystem::path& path);
  static MzImage parse(Bytes fileBytes, std::string name);

  const std::string& name() const { return name_; }
  /// The load image (file contents after the MZ header).
  ByteView image() const { return image_; }
  u16 entryCs() const { return entryCs_; }
  u16 entryIp() const { return entryIp_; }

  /// Segments referenced through relocations, sorted by value, with derived lengths.
  const std::vector<Segment>& segments() const { return segments_; }
  /// Bytes of a segment, addressed by its segment value. Throws DataError if unknown.
  ByteView segment(u16 value) const;
  /// Bytes starting at seg:offset (any 16-bit far pointer), to the end of the image.
  ByteView far(u16 seg, u16 offset) const;

 private:
  std::string name_;
  Bytes file_;
  ByteView image_;
  u16 entryCs_ = 0;
  u16 entryIp_ = 0;
  std::vector<Segment> segments_;
};

}  // namespace encore
