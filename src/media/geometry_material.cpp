#include <common/string.h>
#include <common/property_map.h>
#include <common/named_dictionary.h>

#include <media/geometry.h>

#include <vector>

using namespace engine::media::geometry;
using namespace engine::common;

/*
    Material
*/

typedef std::unordered_map<StringHash, Texture, StringHash::Hasher> TextureDict;
typedef std::vector<TextureDict::iterator> TextureArray;

struct Material::Impl
{
  PropertyMap properties; //shader properties
  TextureDict textures_dict; //dictionary of textures
  TextureArray textures_list; //list of textures
  std::string shader_tags; //shader tags
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

const PropertyMap& Material::properties() const
{
  return impl->properties;
}

PropertyMap& Material::properties()
{
  return impl->properties;
}

size_t Material::textures_count() const
{
  return impl->textures_list.size();
}

size_t Material::add_texture(const char* name, const char* file_name)
{
  return add_texture(Texture(name, file_name));
}

size_t Material::add_texture(const Texture& texture)
{
  auto result = impl->textures_dict.insert(std::make_pair(StringHash(texture.name), texture));

  if (!result.second)
  {
    size_t index = 0;
    for (auto& texture_it : impl->textures_list)
    {
      if (texture_it == result.first)
        return index;

      index++;
    }
  }

  impl->textures_list.push_back(result.first);

  return impl->textures_list.size() - 1;
}

void Material::remove_texture(size_t index)
{
  if (index >= impl->textures_list.size())
    return;

  auto it = impl->textures_list[index];

  impl->textures_list.erase(impl->textures_list.begin() + index);
  impl->textures_dict.erase(it);
}

void Material::remove_texture(const char* name)
{
  if (!name)
    return;

  auto it = impl->textures_dict.find(StringHash(name));

  if (it == impl->textures_dict.end())
    return;

  for (auto it2 = impl->textures_list.begin(); it2 != impl->textures_list.end(); ++it2)
    if (*it2 == it)
    {
      impl->textures_list.erase(it2);
      break;
    }
}

Texture* Material::find_texture(const char* name) const
{
  if (!name)
    return nullptr;

  auto it = impl->textures_dict.find(StringHash(name));

  if (it == impl->textures_dict.end())
    return nullptr;

  return &it->second;
}

Texture& Material::get_texture(size_t index) const
{
  if (index >= impl->textures_list.size())
    throw Exception::format("Texture index '%d' is out of range", index);

  return impl->textures_list[index]->second;
}

/*
    Texture
*/

Texture::Texture(const char* name, const char* file_name)
{
  engine_check_null(name);
  engine_check_null(file_name);

  this->name = name;
  this->file_name = file_name;
}

/*
    MaterialList
*/

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
