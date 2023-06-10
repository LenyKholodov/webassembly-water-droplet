#pragma once

#include <cstdint>
#include <memory>

namespace engine {
namespace media {
namespace image {

/// Color type
struct Color
{
  uint8_t r, g, b, a; /// color components
};

/// Image (only 2d images of RGBA8 format supported)
class Image
{
  public:
    /// Constructor
    Image(const char* path);

    /// Dimensions
    unsigned int width() const;
    unsigned int height() const;

    /// Get data
    const Color* bitmap () const;
    Color* bitmap ();

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}}
