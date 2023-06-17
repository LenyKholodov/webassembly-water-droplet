#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

struct Material::Impl
{
  PropertyMap properties; //shader properties
  TextureList textures; //texture maps
};

Material::Material()
  : impl(std::make_shared<Impl>())
{
}

const TextureList& Material::textures() const
{
  return impl->textures;
}

const PropertyMap& Material::properties() const
{
  return impl->properties;
}

void Material::set_properties(const PropertyMap& properties)
{
  impl->properties = properties;
}
