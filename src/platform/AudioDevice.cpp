#include "platform/AudioDevice.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "core/Log.h"

namespace pfr {

AudioDevice::~AudioDevice() { close(); }

bool AudioDevice::open(int sampleRate) {
  rate_ = sampleRate;
  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;
  spec.freq = sampleRate;
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioDevice::callback, this);
  if (!stream_) {
    log::error(std::string("SDL_OpenAudioDeviceStream: ") + SDL_GetError());
    return false;
  }
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void AudioDevice::close() {
  if (stream_) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
}

void AudioDevice::setSource(Source source) {
  std::lock_guard lock(mutex_);
  source_ = std::move(source);
}

void SDLCALL AudioDevice::callback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int) {
  auto* self = static_cast<AudioDevice*>(userdata);
  const int frames = additionalAmount / static_cast<int>(sizeof(float) * 2);
  if (frames <= 0) return;
  self->scratch_.resize(static_cast<std::size_t>(frames) * 2);
  {
    std::lock_guard lock(self->mutex_);
    if (self->source_)
      self->source_(self->scratch_.data(), frames);
    else
      std::fill(self->scratch_.begin(), self->scratch_.end(), 0.0f);
  }
  SDL_PutAudioStreamData(stream, self->scratch_.data(), frames * static_cast<int>(sizeof(float) * 2));
}

}  // namespace pfr
