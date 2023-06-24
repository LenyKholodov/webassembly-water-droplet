#include <common/exception.h>
#include <common/log.h>
#include "shared.h"

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

namespace
{

const char* MUSIC_PATH = "sounds/music.mp3";
const char* SOUND_DROPLET_GROUND_PATH = "sounds/177156__abstudios__water-drop.wav";
const char* SOUND_DROPLET_LEAF_PATH = "sounds/267221__gkillhour__water-droplet.wav";

const float MUSIC_VOLUME = 1.f;
const clock_t MUSIC_PLAY_TIME = 200 * CLOCKS_PER_SEC; //dirty hack, I know)

}

/// SoundPlayer implementation
struct SoundPlayer::Impl
{
  bool music_playing = false;
  clock_t music_start_play_time = 0;

  Impl()
  {
    EM_ASM({
      var audio = new Audio();

      if (!audio.canPlayType("audio/mpeg")) {
        console.error("Can't play background music, format not supported");
      }

      if (!audio.canPlayType("audio/wav") && !audio.canPlayType("audio/x-wav")) {
        console.error("Can't play sfx, format not supported");
      }
    });
  }

  ~Impl()
  {
    //TODO stop sounds
  }

  static void play_sound(const char* path, float volume, bool is_music = false)
  {
    //this method is always called with constants paths, so no need to check for null path here

    EM_ASM({
      if (Module.isMusicPlaying && $3)
      {
        return;
      }

      var path = Module.UTF8ToString($0, $1);
      var audio = new Audio(path);

      audio.volume = $2;

      var promise = audio.play();
      
      if (promise !== undefined) {
        promise.then(() => {
          if ($3)
          {
            Module.isMusicPlaying = true;
          }
        }).catch(error => console.error);
      }
    }, path, strlen(path), volume, is_music);
  }

  void play_music(bool force)
  {
    if (music_playing && !force)
      return;

    play_sound(MUSIC_PATH, MUSIC_VOLUME, true);

    music_playing = true;
    music_start_play_time = clock();
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

void SoundPlayer::play_music(bool force) const
{
  impl->play_music(force);
}

void SoundPlayer::play_sound(SoundId sound_id, float volume)
{
  Impl::play_sound(sound_id, volume);
}

void SoundPlayer::update()
{
  //engine_log_debug("update sound %u; %d", clock() - impl->music_start_play_time, impl->music_playing);

  if (impl->music_playing && clock() - impl->music_start_play_time > MUSIC_PLAY_TIME)
  {
    engine_log_debug("Restarting music");
    impl->music_playing = false;
    //play_music(true);
  }
}
