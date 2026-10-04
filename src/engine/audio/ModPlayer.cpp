#include "engine/audio/ModPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace encore {
namespace {

// Amiga PAL clock / 2: sample rate for a period p is kPaulaClock / p.
constexpr double kPaulaClock = 7093789.2 / 2.0;

// ProTracker period table, 16 finetunes x 36 notes (C-1 .. B-3).
constexpr i16 kPeriods[16][36] = {
    {856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113},
    {850, 802, 757, 715, 674, 637, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225, 213, 201, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113},
    {844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112},
    {838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 111},
    {832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110},
    {826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109},
    {820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109},
    {814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108},
    {907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120},
    {900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119},
    {894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118},
    {887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118},
    {881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117},
    {875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116},
    {868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115},
    {862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114},
};

constexpr u8 kSine[32] = {0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
                          255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24};

int finetuneIndex(int finetune) { return finetune < 0 ? finetune + 16 : finetune; }

/// Finds the note index (0..35) whose finetune-0 period matches `period` (nearest).
int noteIndexFor(int period) {
  int best = 0;
  int bestDiff = 1 << 30;
  for (int i = 0; i < 36; ++i) {
    const int d = std::abs(kPeriods[0][i] - period);
    if (d < bestDiff) { bestDiff = d; best = i; }
  }
  return best;
}

int periodForNote(int finetune, int noteIndex) {
  noteIndex = std::clamp(noteIndex, 0, 35);
  return kPeriods[finetuneIndex(finetune)][noteIndex];
}

}  // namespace

ModPlayer::ModPlayer(int outputRate) : rate_(outputRate) {
  ch_[0].pan = 0.25f;
  ch_[1].pan = 0.75f;
  ch_[2].pan = 0.75f;
  ch_[3].pan = 0.25f;
  samplesPerTick_ = rate_ * 2.5 / bpm_;
}

void ModPlayer::setModule(const ModFile* mod) {
  std::scoped_lock lock(mutex_);
  mod_ = mod;
  playing_ = false;
  for (auto& c : ch_) c = Channel{.pan = c.pan};
  speed_ = 6;
  bpm_ = 125;
  samplesPerTick_ = rate_ * 2.5 / bpm_;
  order_ = 0;
  row_ = 0;
  tickCounter_ = 0;
  tickAccumulator_ = 0;
  patternDelay_ = 0;
  breakRow_ = -1;
  jumpOrder_ = -1;
}

void ModPlayer::play() {
  std::scoped_lock lock(mutex_);
  if (!mod_) return;
  playing_ = true;
}

void ModPlayer::stop() {
  std::scoped_lock lock(mutex_);
  playing_ = false;
  for (auto& c : ch_)
    if (!c.sfx) c.active = false;
}

void ModPlayer::setPosition(int order) {
  std::scoped_lock lock(mutex_);
  if (!mod_) return;
  order_ = std::clamp(order, 0, std::max(0, mod_->songLength - 1));
  row_ = 0;
  tickCounter_ = 0;
  tickAccumulator_ = 0;
  breakRow_ = -1;
  jumpOrder_ = -1;
  patternDelay_ = 0;
}

void ModPlayer::setStep(Channel& c, int period) {
  if (period <= 0) { c.step = 0; return; }
  c.step = kPaulaClock / period / rate_;
}

void ModPlayer::triggerSample(int channel, int sampleIndex, int period, int volume) {
  std::scoped_lock lock(mutex_);
  if (!mod_ || channel < 0 || channel >= kChannels || sampleIndex < 1 || sampleIndex > 31) return;
  const ModSample& s = mod_->samples[sampleIndex - 1];
  if (s.length == 0) return;
  Channel& c = ch_[channel];
  c.sample = &s;
  c.pos = 0;
  c.period = period > 0 ? period : periodForNote(s.finetune, 12);
  c.finetune = s.finetune;
  c.volume = std::clamp(volume, 0, 64);
  c.active = true;
  c.sfx = true;
  c.effect = 0;
  c.param = 0;
  c.vibratoPos = 0;
  c.tremoloPos = 0;
  setStep(c, c.period);
}

void ModPlayer::triggerNote(int channel, int sampleIndex, int noteIndex, int volume) {
  if (!mod_ || sampleIndex < 1 || sampleIndex > 31) return;
  const int finetune = mod_->samples[static_cast<std::size_t>(sampleIndex) - 1].finetune;
  triggerSample(channel, sampleIndex, periodForNote(finetune, noteIndex - 1), volume);
}

void ModPlayer::stopSample(int channel) {
  std::scoped_lock lock(mutex_);
  if (channel < 0 || channel >= kChannels) return;
  ch_[channel].active = false;
  ch_[channel].sfx = false;
}

void ModPlayer::triggerNote(Channel& c, const ModNote& note, int) {
  if (note.sample) {
    const ModSample& s = mod_->samples[note.sample - 1];
    c.sample = &s;
    c.volume = s.volume;
    c.finetune = s.finetune;
  }
  const bool tonePorta = note.effect == 3 || note.effect == 5;
  if (note.period) {
    const int idx = noteIndexFor(note.period);
    const int period = periodForNote(c.finetune, idx);
    if (tonePorta) {
      c.targetPeriod = period;
    } else {
      c.period = period;
      c.arpeggioBase = idx;
      if (c.vibratoWave < 4) c.vibratoPos = 0;
      if (c.tremoloWave < 4) c.tremoloPos = 0;
      c.pos = 0;
      c.active = c.sample != nullptr && c.sample->length > 0;
      c.sfx = false;
      setStep(c, c.period);
    }
  }
}

void ModPlayer::processRow() {
  const int pattern = mod_->order[order_];
  for (int i = 0; i < kChannels; ++i) {
    Channel& c = ch_[i];
    const ModNote& n = mod_->note(pattern, row_, i);
    c.effect = n.effect;
    c.param = n.param;
    c.pendingNote = nullptr;

    if (n.effect == 0xE && (n.param >> 4) == 0xD) {  // note delay
      c.noteDelay = n.param & 0x0f;
      c.pendingNote = &n;
      if (n.sample) {  // sample/volume change still happens immediately
        const ModSample& s = mod_->samples[n.sample - 1];
        c.sample = &s;
        c.volume = s.volume;
        c.finetune = s.finetune;
      }
      continue;
    }
    if (n.effect == 9 && n.param) c.sampleOffsetMemory = n.param;
    // Music notes reclaim a channel used by a sound effect.
    if (n.period || n.sample) c.sfx = false;
    triggerNote(c, n, i);
    if (n.effect == 9 && n.period && c.sample) {
      c.pos = std::min<double>(c.sampleOffsetMemory * 256, c.sample->length);
    }

    switch (n.effect) {
      case 0x3:
        if (n.param) c.portaSpeed = n.param;
        break;
      case 0x4:
        if (n.param & 0xf0) c.vibratoSpeed = n.param >> 4;
        if (n.param & 0x0f) c.vibratoDepth = n.param & 0x0f;
        break;
      case 0x7:
        if (n.param & 0xf0) c.tremoloSpeed = n.param >> 4;
        if (n.param & 0x0f) c.tremoloDepth = n.param & 0x0f;
        break;
      case 0xB:
        jumpOrder_ = n.param;
        breakRow_ = 0;
        break;
      case 0xC:
        c.volume = std::min<int>(n.param, 64);
        break;
      case 0xD:
        breakRow_ = std::min((n.param >> 4) * 10 + (n.param & 0x0f), 63);
        break;
      case 0xE:
        switch (n.param >> 4) {
          case 0x1: c.period = std::max(113, c.period - (n.param & 0x0f)); setStep(c, c.period); break;
          case 0x2: c.period = std::min(856, c.period + (n.param & 0x0f)); setStep(c, c.period); break;
          case 0x4: c.vibratoWave = n.param & 0x07; break;
          case 0x5: {
            int ft = n.param & 0x0f;
            c.finetune = ft >= 8 ? ft - 16 : ft;
            break;
          }
          case 0x6:
            if ((n.param & 0x0f) == 0) {
              c.loopRow = row_;
            } else if (c.loopCount == 0) {
              c.loopCount = n.param & 0x0f;
              breakRow_ = c.loopRow;
              jumpOrder_ = order_;
            } else if (--c.loopCount > 0) {
              breakRow_ = c.loopRow;
              jumpOrder_ = order_;
            }
            break;
          case 0x7: c.tremoloWave = n.param & 0x07; break;
          case 0xA: c.volume = std::min(64, c.volume + (n.param & 0x0f)); break;
          case 0xB: c.volume = std::max(0, c.volume - (n.param & 0x0f)); break;
          case 0xE: patternDelay_ = n.param & 0x0f; break;
          default: break;
        }
        break;
      case 0xF:
        if (n.param == 0) break;
        if (n.param < 32) speed_ = n.param;
        else { bpm_ = n.param; samplesPerTick_ = rate_ * 2.5 / bpm_; }
        break;
      default: break;
    }
  }
}

void ModPlayer::processEffectsTick() {
  for (int i = 0; i < kChannels; ++i) {
    Channel& c = ch_[i];
    if (c.pendingNote && tickCounter_ == c.noteDelay) {
      c.sfx = false;
      triggerNote(c, *c.pendingNote, i);
      c.pendingNote = nullptr;
    }
    if (c.sfx) continue;
    auto volumeSlide = [&](u8 p) {
      if (p & 0xf0) c.volume = std::min(64, c.volume + (p >> 4));
      else c.volume = std::max(0, c.volume - (p & 0x0f));
    };
    auto tonePortamento = [&]() {
      if (!c.targetPeriod) return;
      if (c.period < c.targetPeriod) c.period = std::min(c.period + c.portaSpeed, c.targetPeriod);
      else if (c.period > c.targetPeriod) c.period = std::max(c.period - c.portaSpeed, c.targetPeriod);
      setStep(c, c.period);
    };
    auto vibrato = [&]() {
      int idx = (c.vibratoPos >> 2) & 0x1f;
      int delta;
      switch (c.vibratoWave & 3) {
        case 1: delta = 255 - idx * 8; if (c.vibratoPos < 0) delta = -delta; break;
        case 2: delta = 255; break;
        default: delta = kSine[idx]; break;
      }
      delta = delta * c.vibratoDepth / 128;
      if (c.vibratoPos < 0) delta = -delta;
      setStep(c, c.period + delta);
      c.vibratoPos += c.vibratoSpeed;
      if (c.vibratoPos > 31) c.vibratoPos -= 64;
    };
    switch (c.effect) {
      case 0x0:
        if (c.param) {
          const int off = tickCounter_ % 3;
          const int semis = off == 0 ? 0 : (off == 1 ? (c.param >> 4) : (c.param & 0x0f));
          setStep(c, periodForNote(c.finetune, c.arpeggioBase + semis));
        }
        break;
      case 0x1: c.period = std::max(113, c.period - c.param); setStep(c, c.period); break;
      case 0x2: c.period = std::min(856, c.period + c.param); setStep(c, c.period); break;
      case 0x3: tonePortamento(); break;
      case 0x4: vibrato(); break;
      case 0x5: tonePortamento(); volumeSlide(c.param); break;
      case 0x6: vibrato(); volumeSlide(c.param); break;
      case 0x7: {
        int idx = (c.tremoloPos >> 2) & 0x1f;
        int delta = (c.tremoloWave & 3) == 1 ? 255 - idx * 8 : ((c.tremoloWave & 3) == 2 ? 255 : kSine[idx]);
        delta = delta * c.tremoloDepth / 64;
        if (c.tremoloPos < 0) delta = -delta;
        // Tremolo modulates the mixed volume only; the base volume is untouched.
        c.tremoloPos += c.tremoloSpeed;
        if (c.tremoloPos > 31) c.tremoloPos -= 64;
        break;
      }
      case 0xA: volumeSlide(c.param); break;
      case 0xE:
        switch (c.param >> 4) {
          case 0x9:
            if ((c.param & 0x0f) && tickCounter_ % (c.param & 0x0f) == 0) { c.pos = 0; c.active = c.sample != nullptr; }
            break;
          case 0xC:
            if (tickCounter_ == (c.param & 0x0f)) c.volume = 0;
            break;
          default: break;
        }
        break;
      default: break;
    }
  }
}

void ModPlayer::tick() {
  if (!mod_ || !playing_ || mod_->songLength == 0) return;
  if (tickCounter_ == 0) {
    if (patternDelay_ > 0) {
      --patternDelay_;
    } else {
      processRow();
    }
  } else {
    processEffectsTick();
  }
  ++tickCounter_;
  if (tickCounter_ >= speed_) {
    tickCounter_ = 0;
    if (patternDelay_ > 0) return;  // repeat this row's tick 0 without re-triggering effects
    if (breakRow_ >= 0) {
      row_ = breakRow_;
      breakRow_ = -1;
      if (jumpOrder_ >= 0) { order_ = jumpOrder_; jumpOrder_ = -1; }
      else ++order_;
    } else if (++row_ >= 64) {
      row_ = 0;
      ++order_;
    }
    if (order_ >= mod_->songLength) order_ = mod_->restart < mod_->songLength ? mod_->restart : 0;
  }
}

void ModPlayer::mix(float* out, int frames) {
  for (Channel& c : ch_) {
    if (!c.active || !c.sample) continue;
    if (!musicEnabled_ && !c.sfx) continue;
    const ModSample& s = *c.sample;
    const int end = s.loops() ? static_cast<int>(s.loopStart + s.loopLength) : static_cast<int>(s.length);
    const float gainL = master_ * (c.volume / 64.0f) * (1.0f - c.pan) * 0.5f;
    const float gainR = master_ * (c.volume / 64.0f) * c.pan * 0.5f;
    for (int i = 0; i < frames; ++i) {
      if (c.pos >= end) {
        if (s.loops()) { c.pos -= s.loopLength; if (c.pos >= end) c.pos = s.loopStart; }
        else { c.active = false; c.sfx = false; break; }
      }
      const int p0 = static_cast<int>(c.pos);
      float v;
      if (interpolate_) {
        int p1 = p0 + 1;
        if (p1 >= end) p1 = s.loops() ? static_cast<int>(s.loopStart) : p0;
        const float frac = static_cast<float>(c.pos - p0);
        v = (s.data[p0] * (1.0f - frac) + s.data[p1] * frac) / 128.0f;
      } else {
        v = s.data[p0] / 128.0f;
      }
      out[i * 2] += v * gainL;
      out[i * 2 + 1] += v * gainR;
      c.pos += c.step;
    }
  }
}

void ModPlayer::render(float* out, int frames) {
  std::scoped_lock lock(mutex_);
  std::memset(out, 0, sizeof(float) * frames * 2);
  if (!mod_) return;
  int done = 0;
  while (done < frames) {
    if (tickAccumulator_ <= 0) {
      tick();
      tickAccumulator_ += samplesPerTick_;
    }
    const int chunk = std::min(frames - done, std::max(1, static_cast<int>(std::ceil(tickAccumulator_))));
    mix(out + done * 2, chunk);
    tickAccumulator_ -= chunk;
    done += chunk;
  }
}

}  // namespace encore
