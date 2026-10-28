#pragma once
#include <cstddef>
#include <cstdint>

namespace sf
{
  using Int16 = std::int16_t;
  class SoundBuffer
  {
  public:
    bool loadFromSamples(const Int16 *, std::size_t, unsigned, unsigned) { return true; }
  };
  class Sound
  {
  public:
    void setBuffer(const SoundBuffer &) {}
    void setVolume(float) {}
    void setPitch(float) {}
    void play() {}
  };
}
