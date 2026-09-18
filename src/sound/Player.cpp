#include "sound/Player.h"

#include <algorithm>

#include "sound/Periods.h"

namespace pfr {

namespace {
constexpr u8 kVibratoLut[32] = {0x00, 0x18, 0x31, 0x4a, 0x61, 0x78, 0x8d, 0xa1, 0xb4, 0xc5, 0xd4,
                                0xe0, 0xeb, 0xf4, 0xfa, 0xfd, 0xff, 0xfd, 0xfa, 0xf4, 0xeb, 0xe0,
                                0xd4, 0xc5, 0xb4, 0xa1, 0x8d, 0x78, 0x61, 0x4a, 0x31, 0x18};
}

Player::Player(Mod module, std::shared_ptr<Sequencer> sequencer, int sampleRate)
    : module_(std::move(module)),
      sequencer_(std::move(sequencer)),
      sampleRate_(static_cast<u32>(sampleRate)),
      samplesInTick_(static_cast<u32>(sampleRate) / 50) {
  position_ = sequencer_->nextPosition();
}

void Player::playSfx(const Sfx& sfx, u8 volume) {
  sfx_.store(u32{sfx.period} | u32{sfx.sample} << 8 | u32{volume} << 16 | u32{sfx.channel} << 24,
             std::memory_order_relaxed);
}

void Player::setStep(Channel& c, u16 period) const {
  const u32 byteLen = 0x361f0f / period;
  c.bytesPerFrame = (u64{byteLen} << 32) / sampleRate_;
}

void Player::render(float* out, int frames) {
  if (paused()) {
    std::fill(out, out + frames * 2, 0.0f);
    return;
  }
  const i64 master = masterVolume();
  processInterrupt();
  if (const u32 sfx = sfx_.exchange(0, std::memory_order_relaxed); sfx != 0) {
    SongNote n;
    n.period = static_cast<u8>(sfx & 0xff);
    n.sample = static_cast<u8>(sfx >> 8 & 0xff);
    const u8 vol = static_cast<u8>(sfx >> 16 & 0xff);
    if (vol != 0) {
      n.volume = SongNote::Volume::Set;
      n.volumeArg = vol;
    }
    playNote(std::min<std::size_t>(sfx >> 24 & 0xff, 3), n);
  }
  for (int i = 0; i < frames; ++i) {
    if (samplesLeft_ == 0) {
      if (ticksLeft_ == 0) {
        playRow();
        ticksLeft_ = static_cast<u8>(speed_ - 1);
      } else {
        --ticksLeft_;
        playEffects();
      }
      samplesLeft_ = samplesInTick_;
      ticks_.fetch_add(1, std::memory_order_release);
    }
    const i64 l = (static_cast<i64>(playChannel(0)) + playChannel(1)) / 0x100 * master;
    const i64 r = (static_cast<i64>(playChannel(2)) + playChannel(3)) / 0x100 * master;
    out[i * 2] = static_cast<float>(static_cast<double>(l) / 2147483648.0);
    out[i * 2 + 1] = static_cast<float>(static_cast<double>(r) / 2147483648.0);
    --samplesLeft_;
  }
}

void Player::processInterrupt() {
  if (auto pos = sequencer_->checkInterrupt()) {
    position_ = *pos;
    row_ = 0;
    ticksLeft_ = 0;
    samplesLeft_ = 0;
  }
}

void Player::playRow() {
  if (position_ >= module_.positions.size()) position_ = 0;
  const auto& row = module_.patterns[module_.positions[position_]][row_];
  for (std::size_t i = 0; i < 4; ++i) playNote(i, row[i]);
  if (jump_) {
    position_ = *jump_;
    row_ = 0;
    jump_.reset();
  } else if (patternBreak_) {
    row_ = *patternBreak_;
    position_ = sequencer_->nextPosition();
    patternBreak_.reset();
  } else if (++row_ == 0x40) {
    row_ = 0;
    position_ = sequencer_->nextPosition();
  }
}

void Player::playNote(std::size_t ci, const SongNote& note) {
  Channel& c = channels_[ci];
  if (note.sample && *note.sample < module_.samples.size()) {
    c.sample = *note.sample;
    c.samplePosReload = 0;
    c.volume = module_.samples[c.sample].volume;
  }
  const SongSample& s = module_.samples[c.sample];
  const u8 ft = s.finetune & 0xf;
  if (note.period) {
    c.xperiod = *note.period;
    c.period = kPeriods[ft][c.xperiod];
    c.samplePos = c.samplePosReload;
    c.vibPhase = 0;
    setStep(c, c.period);
  }
  switch (note.tone) {
    case SongNote::Tone::None: c.tone = ToneFx::None; break;
    case SongNote::Tone::Arpeggio:
      c.tone = ToneFx::Arpeggio;
      c.arpeggio[0] = kPeriods[ft][std::min(c.xperiod + note.toneA, 35)];
      c.arpeggio[1] = kPeriods[ft][std::min(c.xperiod + note.toneB, 35)];
      break;
    case SongNote::Tone::Portamento:
      c.tone = ToneFx::Portamento;
      if (note.portaTarget) c.portaTarget = kPeriods[ft][*note.portaTarget];
      if (note.portaSpeed) c.portaSpeed = *note.portaSpeed;
      break;
    case SongNote::Tone::Vibrato:
      c.tone = ToneFx::Vibrato;
      if (note.vibRate) c.vibRate = static_cast<u8>(*note.vibRate * 4);
      if (note.vibDepth) c.vibDepth = *note.vibDepth;
      break;
  }
  switch (note.volume) {
    case SongNote::Volume::None: c.volumeSlide = false; break;
    case SongNote::Volume::Set: c.volumeSlide = false; c.volume = static_cast<u8>(note.volumeArg); break;
    case SongNote::Volume::Slide: c.volumeSlide = true; c.slideSpeed = note.volumeArg; break;
    case SongNote::Volume::Reset: c.volumeSlide = false; c.volume = s.volume; break;
  }
  switch (note.misc) {
    case SongNote::Misc::None: break;
    case SongNote::Misc::SetSampleOffset:
      c.samplePosReload = u64{note.miscArg} << 40;
      if (note.sample) c.samplePos = c.samplePosReload;
      break;
    case SongNote::Misc::PositionJump: jump_ = sequencer_->jump(note.miscArg); break;
    case SongNote::Misc::PatternBreak: patternBreak_ = note.miscArg; break;
    case SongNote::Misc::RetrigNote:
      c.tone = ToneFx::Retrig;
      c.retrigPeriod = note.miscArg;
      c.retrigLeft = static_cast<u8>(note.miscArg - 1);
      break;
    case SongNote::Misc::SetSpeed:
      if (note.miscArg != 0) {
        speed_ = note.miscArg;
        ticksLeft_ = static_cast<u8>(speed_ - 1);
      }
      break;
  }
}

void Player::playEffects() {
  for (Channel& c : channels_) {
    switch (c.tone) {
      case ToneFx::None: break;
      case ToneFx::Arpeggio: {
        const u16 tmp = c.period;
        c.period = c.arpeggio[1];
        c.arpeggio[1] = c.arpeggio[0];
        c.arpeggio[0] = tmp;
        if (c.period) setStep(c, c.period);
        break;
      }
      case ToneFx::Portamento:
        if (c.portaTarget != 0) {
          if (c.portaTarget < c.period) {
            c.period = static_cast<u16>(c.period - c.portaSpeed);
            if (c.period < c.portaTarget) c.period = c.portaTarget;
          } else {
            c.period = static_cast<u16>(c.period + c.portaSpeed);
            if (c.period > c.portaTarget) c.period = c.portaTarget;
          }
          setStep(c, c.period);
        }
        break;
      case ToneFx::Vibrato: {
        const u8 phase = c.vibPhase;
        c.vibPhase = static_cast<u8>(phase + c.vibRate);
        i16 delta = static_cast<i16>(kVibratoLut[phase >> 2 & 0x1f] * c.vibDepth);
        delta = static_cast<i16>(delta >> 7);
        if (phase & 0x80) delta = static_cast<i16>(-delta);
        const u16 period = static_cast<u16>(c.period + delta);
        if (period != 0) setStep(c, period);
        break;
      }
      case ToneFx::Retrig:
        if (c.retrigLeft == 0) {
          c.retrigLeft = static_cast<u8>(c.retrigPeriod - 1);
          c.samplePos = 0;
        } else {
          --c.retrigLeft;
        }
        break;
    }
    if (c.volumeSlide) c.volume = static_cast<u8>(std::clamp(c.volume + c.slideSpeed, 0, 0x40));
  }
}

i32 Player::playChannel(std::size_t ci) {
  Channel& c = channels_[ci];
  const SongSample& s = module_.samples[c.sample];
  std::size_t pos = static_cast<std::size_t>(c.samplePos >> 32);
  if (s.repeat && s.repeat->second > 0) {
    const auto [rs, rl] = *s.repeat;
    while (pos >= rs + rl) {
      pos -= rl;
      c.samplePos -= u64{rl} << 32;
    }
  }
  if (pos >= s.data.size()) return 0;
  c.samplePos += c.bytesPerFrame;
  return static_cast<i32>(static_cast<i8>(s.data[pos])) * 65536 * c.volume;
}

}  // namespace pfr
