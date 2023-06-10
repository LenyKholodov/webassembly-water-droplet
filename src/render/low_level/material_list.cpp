#include "shared.h"

using namespace engine::common;
using namespace engine::render::low_level;

typedef NamedDictionary<Material> MaterialDict;

/// Internal implementation of material library
struct MaterialList::Impl
{
  MaterialDict materials; //dictionary of materials
};

MaterialList::MaterialList()
  : impl(std::make_shared<Impl>())
{
}

size_t MaterialList::count() const
{
  return impl->materials.size();
}

void MaterialList::insert(const char* name, const Material& material)
{
  engine_check_null(name);

  impl->materials.insert(name, material);
}

void MaterialList::remove(const char* name)
{
  impl->materials.erase(name);
}

Material* MaterialList::find(const char* name) const
{
  return impl->materials.find(name);
}

Material& MaterialList::get(const char* name) const
{
  if (Material* material = find(name))
    return *material;

  throw Exception::format("Material '%s' has not been found", name);
}
