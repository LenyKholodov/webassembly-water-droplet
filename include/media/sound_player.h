#pragma once

#include <memory>

namespace engine {
namespace media {
namespace sound {

enum class SoundId
{
  drop
};

/// Sound player
class SoundPlayer
{
  public:
    /// Constructor
    SoundPlayer();

    /// Play music
    void play_music() const;
    
    /// Play sound
    void play_sound(SoundId sound_id) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}}
