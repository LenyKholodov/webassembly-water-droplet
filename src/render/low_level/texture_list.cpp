#include "shared.h"

using namespace engine::common;
using namespace engine::render::low_level;

typedef NamedDictionary<Texture> TextureDict;

/// Internal implementation of texture library
struct TextureList::Impl
{
  TextureDict textures; //dictionary of textures
};

TextureList::TextureList()
  : impl(std::make_shared<Impl>())
{
}

size_t TextureList::count() const
{
  return impl->textures.size();
}

void TextureList::insert(const char* name, const Texture& texture)
{
  engine_check_null(name);

  impl->textures.insert(name, texture);
}

void TextureList::remove(const char* name)
{
  impl->textures.erase(name);
}

Texture* TextureList::find(const char* name) const
{
  return impl->textures.find(name);
}

Texture& TextureList::get(const char* name) const
{
  if (Texture* texture = find(name))
    return *texture;

  throw Exception::format("Texture '%s' has not been found", name);
}
