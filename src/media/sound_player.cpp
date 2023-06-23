#include <common/exception.h>
#include <media/sound_player.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

using namespace engine::media::sound;

namespace
{

const char* MUSIC_PATH = "sounds/music.mp3";
const char* SOUND_DROPLET_GROUND_PATH = "sounds/177156__abstudios__water-drop.wav";
const char* SOUND_DROPLET_LEAF_PATH = "sounds/267221__gkillhour__water-droplet.wav";

const float MUSIC_VOLUME = 1.f;

}

/// SoundPlayer implementation
struct SoundPlayer::Impl
{
  bool music_playing = false;

  Impl()
  {
  }

  ~Impl()
  {
    //TODO stop sounds
  }

  static void play_sound(const char* path, float volume)
  {
    //this method is always called with constants paths, so no need to check for null path here

    EM_ASM({
      var path = Module.UTF8ToString($0, $1);
      var audio = new Audio(path);
      audio.volume = $2;
      audio.play();          
    }, path, strlen(path), volume);
  }

  void play_music()
  {
    if (music_playing)
      return;

    play_sound(MUSIC_PATH, MUSIC_VOLUME);

    music_playing = true;
  }

  static void play_sound(SoundId sound_id, float volume)
  {
    const char* sound_path;

    switch(sound_id)
    {
      case SoundId::droplet_ground:
        sound_path = SOUND_DROPLET_GROUND_PATH;
        break;
      case SoundId::droplet_leaf:
        sound_path = SOUND_DROPLET_LEAF_PATH;
        break;
      default:
        throw engine::common::Exception::format("Unknown sound id: %d", static_cast<int>(sound_id));
    }

    play_sound(sound_path, volume);
  }
};

SoundPlayer::SoundPlayer()
  : impl(new Impl())
  {}

void SoundPlayer::play_music() const
{
  impl->play_music();
}

void SoundPlayer::play_sound(SoundId sound_id, float volume)
{
  Impl::play_sound(sound_id, volume);
}
