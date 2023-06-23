#pragma once

#include <memory>

namespace engine {
namespace media {
namespace sound {

enum class SoundId
{
  droplet_ground,
  droplet_leaf
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
    static void play_sound(SoundId sound_id, float volume = 1.f);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}}
