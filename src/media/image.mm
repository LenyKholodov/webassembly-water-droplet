#import <AppKit/NSBitmapImageRep.h>
#import <CoreImage/CIImage.h>

#include <common/exception.h>
#include <media/image.h>

//TODO use cross-platform image loader library

using namespace engine::media::image;
using namespace engine::common;

/// Image implementation
struct Image::Impl
{
  NSBitmapImageRep* bitmap_image;

  Impl(const char* path)
    : bitmap_image(0)
  {
    engine_check_null(path);

    CIImage* ci_image = [CIImage imageWithContentsOfURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path]]];

    if (!ci_image)
      throw Exception::format("Can't load image from path '%s'", path);

    bitmap_image = [[NSBitmapImageRep alloc] initWithCIImage:ci_image];

    engine_check(![bitmap_image isPlanar]);
    engine_check([bitmap_image samplesPerPixel] == 3 || [bitmap_image samplesPerPixel] == 4);
    engine_check([bitmap_image bitsPerSample] == 8);
    engine_check([bitmap_image bitsPerPixel] == 32);

    NSInteger image_width = [bitmap_image pixelsWide];
    NSInteger image_height = [bitmap_image pixelsHigh];

    Color* bitmap = reinterpret_cast<Color*>([bitmap_image bitmapData]);

    if ([bitmap_image bitmapFormat] == NSBitmapFormatAlphaFirst)
    {
        //Image is loaded as ARGB, convert to RGBA
      Color* current_pixel = bitmap;

      for (size_t i = 0, count = image_width * image_height; i < count; i++, current_pixel++)
      {
        std::swap (current_pixel->r, current_pixel->a);
        std::swap (current_pixel->g, current_pixel->r);
        std::swap (current_pixel->b, current_pixel->g);
      }
    }

    //images are loaded flipped, restore correct orientation
    Color temp_buffer[image_width];

    size_t row_size = sizeof(temp_buffer);

    for (size_t i = 0, count = image_height / 2; i < count; i++)
    {
      memcpy(temp_buffer, bitmap + i * image_width, row_size);
      memcpy(bitmap + i * image_width, bitmap + (image_height - i - 1) * image_width, row_size);
      memcpy(bitmap + (image_height - i - 1) * image_width, temp_buffer, row_size);
    }
  }

  ~Impl()
  {
    [bitmap_image release];
  }
};

Image::Image(const char* path)
  : impl(new Impl(path))
  {}

unsigned int Image::width() const
{
  return static_cast<unsigned int>([impl->bitmap_image pixelsWide]);
}

unsigned int Image::height() const
{
  return static_cast<unsigned int>([impl->bitmap_image pixelsHigh]);
}

const Color* Image::bitmap () const
{
  return reinterpret_cast<Color*>([impl->bitmap_image bitmapData]);
}

Color* Image::bitmap ()
{
  return reinterpret_cast<Color*>([impl->bitmap_image bitmapData]);
}
