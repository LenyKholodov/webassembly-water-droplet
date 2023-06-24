#include <common/exception.h>
#include <media/image.h>

#include <SDL2/SDL_image.h>

using namespace engine::media::image;
using namespace engine::common;

/// Image implementation
struct Image::Impl
{
  SDL_Surface* image = nullptr;

  Impl(const char* path)
  {
    engine_check_null(path);

    SDL_Surface* image = IMG_Load(path);

    if (!image)
      throw Exception::format("Failed to load image: '%s'", IMG_GetError());

    if (image->format->format != SDL_PIXELFORMAT_ABGR8888)
    {
      SDL_Surface* converted = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_ABGR8888, 0);

      SDL_FreeSurface(image);

      image = converted;
    }

    this->image = image;
  }

  ~Impl()
  {
    SDL_FreeSurface(image);
  }
};

Image::Image(const char* path)
  : impl(new Impl(path))
  {}

unsigned int Image::width() const
{
  return impl->image->w;
}

unsigned int Image::height() const
{
  return impl->image->h;
}

const Color* Image::bitmap () const
{
  return reinterpret_cast<Color*>(impl->image->pixels);
}

Color* Image::bitmap ()
{
  return reinterpret_cast<Color*>(impl->image->pixels);
}
