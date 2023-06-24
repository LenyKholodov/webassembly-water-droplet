#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

struct Material::Impl
{
  PropertyMap properties; //shader properties
  TextureList textures; //texture maps
  std::string shader_tags;
};

Material::Material()
  : impl(std::make_shared<Impl>())
{
}

const char* Material::shader_tags() const
{
  return impl->shader_tags.c_str();
}

void Material::set_shader_tags(const char* tags)
{
  if (!tags)
    tags = "";

  impl->shader_tags = tags;
}

const TextureList& Material::textures() const
{
  return impl->textures;
}

void Material::set_textures(const TextureList& textures)
{
  impl->textures = textures;
}

const PropertyMap& Material::properties() const
{
  return impl->properties;
}

void Material::set_properties(const PropertyMap& properties)
{
  impl->properties = properties;
}
