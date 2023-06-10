#include <common/property_map.h>
#include <common/named_dictionary.h>

#include <vector>

using namespace engine::common;

typedef std::vector<Property> PropertyArray;
typedef NamedDictionary<size_t> PropertyDict;

/// Implementation details of property map
struct PropertyMap::Impl
{
  PropertyArray properties;
  PropertyDict dictionary;
};

PropertyMap::PropertyMap()
  : impl(std::make_shared<Impl>())
{
}

size_t PropertyMap::count() const
{
  return impl->properties.size();
}

const Property* PropertyMap::items() const
{
  return const_cast<PropertyMap&>(*this).items();
}

Property* PropertyMap::items()
{
  if (!impl->properties.size())
    return nullptr;

  return &impl->properties[0];
}

const Property* PropertyMap::find(const char* name) const
{
  return const_cast<PropertyMap&>(*this).find(name);
}

Property* PropertyMap::find(const char* name)
{
  if (size_t* index = impl->dictionary.find(name))
    return &impl->properties[*index];

  return nullptr;
}

const Property& PropertyMap::get(const char* name) const
{
  return const_cast<PropertyMap&>(*this).get(name);   
}

Property& PropertyMap::get(const char* name)
{
  if (Property* property = find(name))
    return *property;

  throw Exception::format("Property '%s' has not been found", name);
}

size_t PropertyMap::insert(const char* name, const Property& property)
{
  engine_check_null(name);

  if (Property* property = find(name))
    throw Exception::format("Property '%s' has been already inserted", name);

  impl->properties.push_back(property);

  size_t index = impl->properties.size() - 1;

  try
  {
    impl->dictionary.insert(name, index);

    return index;
  }
  catch (...)
  {
    impl->properties.pop_back();
    throw;
  }
}

void PropertyMap::erase(const char* name)
{
  if (!name)
    return;

  size_t* index = impl->dictionary.find(name);

  if (!index)
    return;

  impl->properties.erase(impl->properties.begin() + *index);
  impl->dictionary.erase(name);
}

void PropertyMap::clear()
{
  impl->properties.clear();
  impl->dictionary.clear();    
}
