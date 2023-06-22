#include "shared.h"

#include <vector>

using namespace engine::render::low_level;
using namespace engine::common;

constexpr size_t RESERVED_PASSES_COUNT = 8;

namespace
{

struct PassEntry
{
  Pass pass; //low level rendering pass
  int priority; //rendering priority

  PassEntry(const Pass& pass, int priority)
    : pass(pass)
    , priority(priority)
  {
  }
};

typedef std::unordered_map<StringHash, PassEntry, StringHash::Hasher> PassMap;
typedef std::vector<PassMap::iterator> PassArray;

}

/// Implementation details of pass
struct PassGroup::Impl
{
  PassMap passes; //list of passes
  PassArray pass_array; //array of passes
  common::PropertyMap properties; //pass group properties
  int default_pass = -1;

  Impl()
  {
    pass_array.reserve(RESERVED_PASSES_COUNT);
  }
};

PassGroup::PassGroup()
  : impl(new Impl)
{
}

PassGroup::~PassGroup()
{
}

size_t PassGroup::passes_count() const
{
  return impl->passes.size();
}

const Pass& PassGroup::pass(size_t index) const
{
  engine_check_range(index, impl->pass_array.size());

  return impl->pass_array[index]->second.pass;
}

int PassGroup::pass_priority(size_t index) const
{
  engine_check_range(index, impl->pass_array.size());

  return impl->pass_array[index]->second.priority;
}

size_t PassGroup::add_pass(const char* shader_tags, const Pass& pass, int priority)
{
  if (!shader_tags)
    shader_tags = "";

  StringHash key = StringRef(shader_tags);
  auto it = impl->passes.find(key);

  if (it != impl->passes.end())
  {
    it->second.pass = pass;
    it->second.priority = priority;

    for (PassArray::iterator entry = impl->pass_array.begin(), end = impl->pass_array.end(); entry != end; ++entry)
      if (*entry == it)
        return entry - impl->pass_array.begin();

    engine_check(false && "Unreachable code");
  }

  auto new_entry = impl->passes.emplace(shader_tags, PassEntry(pass, priority));

  impl->pass_array.push_back(new_entry.first);

  return impl->pass_array.size() - 1;
}

void PassGroup::remove_pass(size_t pass_index)
{
  if (pass_index >= impl->pass_array.size())
    return;

  auto it = impl->pass_array[pass_index];

  impl->passes.erase(it);
  impl->pass_array.erase(impl->pass_array.begin() + pass_index);
}

void PassGroup::remove_all_passes()
{
  impl->pass_array.clear();
  impl->passes.clear();
}

PropertyMap& PassGroup::properties() const
{
  return impl->properties;
}

void PassGroup::add_mesh(
  const Mesh& mesh,
  const math::mat4f& model_tm,
  size_t first_primitive,
  size_t primitives_count,
  const PropertyMap& properties,
  const TextureList& textures)
{
  for (size_t i=0, max_count = mesh.primitives_count(); i < primitives_count; i++)
  {
    if (first_primitive + i >= max_count)
      break;
    
    const Primitive& primitive = mesh.primitive(i + first_primitive);
    const Material& material = primitive.material;
    const char* shader_tags = material.shader_tags();

    if (!shader_tags)
      shader_tags = "";

    auto it = impl->passes.find(StringRef(shader_tags));
    PassEntry* entry = nullptr;

    if (it == impl->passes.end())
    {
      if (impl->default_pass < 0 || impl->default_pass >= impl->pass_array.size())
        continue;

      entry = &impl->pass_array[impl->default_pass]->second;
    }
    else entry = &it->second;

    Pass pass = entry->pass;
    int priority = entry->priority;

    pass.add_primitive(primitive, model_tm, properties, textures);
  }
}

int PassGroup::default_pass() const
{
  return impl->default_pass;
}

void PassGroup::set_default_pass(int pass_index)
{
  impl->default_pass = pass_index;
}
