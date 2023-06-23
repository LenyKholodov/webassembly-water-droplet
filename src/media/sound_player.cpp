#include <common/exception.h>
#include <media/sound_player.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

using namespace engine::media::sound;

namespace
{

const char* MUSIC_PATH = "sounds/music.mp3";
const char* SOUND_DROP_PATH = "sounds/177156__abstudios__water-drop.wav";

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

  void play_sound(const char* path)
  {
    //this method is always called with constants paths, so no need to check for null path here

    EM_ASM({
      var path = Module.UTF8ToString($0, $1);
      var audio = new Audio(path);
      audio.play();          
    }, path, strlen(path));
  }

  void play_music()
  {
    if (music_playing)
      return;

    play_sound(MUSIC_PATH);

    music_playing = true;
  }

  void play_sound(SoundId sound_id)
  {
    const char* sound_path;

    switch(sound_id)
    {
      case SoundId::drop:
        sound_path = SOUND_DROP_PATH;
        break;
      default:
        throw engine::common::Exception::format("Unknown sound id: %d", static_cast<int>(sound_id));
    }

    play_sound(sound_path);
  }
};

SoundPlayer::SoundPlayer()
  : impl(new Impl())
  {}

void SoundPlayer::play_music() const
{
  impl->play_music();
}

void SoundPlayer::play_sound(SoundId sound_id) const
{
  impl->play_sound(sound_id);
}
