#include "sound/Mod.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "core/Error.h"
#include "sound/Periods.h"

namespace pfr {

namespace {

std::optional<u8> nonZero(u8 v) { return v ? std::optional<u8>(v) : std::nullopt; }

SongNote decodeNote(u32 value) {
  SongNote n;
  const u16 period = static_cast<u16>(value >> 16 & 0xfff);
  if (period != 0) {
    for (u8 i = 0; i < 36; ++i)
      if (kPeriods[0][i] == period) n.period = i;
    if (!n.period) throw DataError("module note with an unknown period");
  }
  const u8 sample = static_cast<u8>((value >> 24 & 0xf0) | (value >> 12 & 0xf));
  if (sample) n.sample = sample;
  const u16 effect = static_cast<u16>(value & 0xfff);
  const u8 arg = static_cast<u8>(effect & 0xff), hi = arg >> 4, lo = arg & 0xf;
  auto slide = [&] { return hi ? static_cast<int>(hi) : -static_cast<int>(lo); };
  switch (effect >> 8) {
    case 0:
      if (effect != 0) {
        n.tone = SongNote::Tone::Arpeggio;
        n.toneA = hi;
        n.toneB = lo;
      }
      break;
    case 1: n.tone = SongNote::Tone::Portamento; n.portaTarget = 35; n.portaSpeed = nonZero(arg); break;
    case 2: n.tone = SongNote::Tone::Portamento; n.portaTarget = 0; n.portaSpeed = nonZero(arg); break;
    case 3:
      n.tone = SongNote::Tone::Portamento;
      n.portaTarget = n.period;
      n.portaSpeed = nonZero(arg);
      if (n.sample) n.volume = SongNote::Volume::Reset;
      n.period.reset();
      n.sample.reset();
      break;
    case 4: n.tone = SongNote::Tone::Vibrato; n.vibRate = nonZero(hi); n.vibDepth = nonZero(lo); break;
    case 5:
      n.tone = SongNote::Tone::Portamento;
      n.portaTarget = n.period;
      n.volume = SongNote::Volume::Slide;
      n.volumeArg = slide();
      n.period.reset();
      n.sample.reset();
      break;
    case 6:
      n.tone = SongNote::Tone::Vibrato;
      n.volume = SongNote::Volume::Slide;
      n.volumeArg = slide();
      break;
    case 9: n.misc = SongNote::Misc::SetSampleOffset; n.miscArg = arg; break;
    case 0xa: n.volume = SongNote::Volume::Slide; n.volumeArg = slide(); break;
    case 0xb: n.misc = SongNote::Misc::PositionJump; n.miscArg = arg; break;
    case 0xc: n.volume = SongNote::Volume::Set; n.volumeArg = arg; break;
    case 0xd: n.misc = SongNote::Misc::PatternBreak; n.miscArg = arg; break;
    case 0xe:
      if (hi != 9) throw DataError("unsupported module effect");
      n.misc = SongNote::Misc::RetrigNote;
      n.miscArg = lo;
      break;
    case 0xf: n.misc = SongNote::Misc::SetSpeed; n.miscArg = arg; break;
    default: throw DataError("unsupported module effect");
  }
  return n;
}

}  // namespace

Mod Mod::load(ByteView data) {
  if (data.size() <= 1080) throw DataError("module too short");
  Mod m;
  m.name.assign(reinterpret_cast<const char*>(data.data()), 20);
  m.name.resize(m.name.find('\0') == std::string::npos ? 20 : m.name.find('\0'));
  std::vector<std::size_t> lens{0};
  m.samples.emplace_back();
  std::size_t pos = 20;
  for (int i = 0; i < 31; ++i, pos += 30) {
    SongSample s;
    s.name.assign(reinterpret_cast<const char*>(data.data() + pos), 22);
    s.name.resize(s.name.find('\0') == std::string::npos ? 22 : s.name.find('\0'));
    lens.push_back(static_cast<std::size_t>(rd16be(data, pos + 22)) * 2);
    s.finetune = data[pos + 24];
    s.volume = data[pos + 25];
    const std::size_t repPos = static_cast<std::size_t>(rd16be(data, pos + 26)) * 2;
    const std::size_t repLen = static_cast<std::size_t>(rd16be(data, pos + 28)) * 2;
    if (!(repPos == 0 && repLen == 2)) s.repeat = std::make_pair(repPos, repLen);
    m.samples.push_back(std::move(s));
  }
  const u8 songLen = data[pos];
  m.posRestart = data[pos + 1] == 127 ? 0 : data[pos + 1];
  if (songLen == 0 || songLen > 128) throw DataError("bad module song length");
  u8 maxPattern = 0;
  for (int i = 0; i < 128; ++i) maxPattern = std::max(maxPattern, data[pos + 2 + static_cast<std::size_t>(i)]);
  m.positions.assign(data.begin() + static_cast<std::ptrdiff_t>(pos + 2),
                     data.begin() + static_cast<std::ptrdiff_t>(pos + 2 + songLen));
  pos += 134;
  for (int p = 0; p <= maxPattern; ++p, pos += 0x400) {
    if (pos + 0x400 > data.size()) throw DataError("truncated module");
    Pattern pat;
    for (std::size_t r = 0; r < 0x40; ++r)
      for (std::size_t c = 0; c < 4; ++c) pat[r][c] = decodeNote(rd32be(data, pos + (r << 4 | c << 2)));
    m.patterns.push_back(pat);
  }
  for (std::size_t i = 0; i < m.samples.size(); ++i) {
    if (lens[i] <= 2) continue;
    if (pos + lens[i] > data.size()) throw DataError("truncated module sample");
    m.samples[i].data.assign(data.begin() + static_cast<std::ptrdiff_t>(pos),
                             data.begin() + static_cast<std::ptrdiff_t>(pos + lens[i]));
    pos += lens[i];
  }
  return m;
}

}  // namespace pfr
