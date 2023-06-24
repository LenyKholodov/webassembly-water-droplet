#pragma once

#include <common/exception.h>

#include <math/vector.h>
#include <math/matrix.h>

#include <memory>
#include <string>
#include <vector>

namespace engine {
namespace common {

/// Property type
enum PropertyType
{
  PropertyType_Int,
  PropertyType_Float,
  PropertyType_Vec2f,
  PropertyType_Vec3f,
  PropertyType_Vec4f,
  PropertyType_Mat4f,

  PropertyType_IntArray,
  PropertyType_FloatArray,
  PropertyType_Vec2fArray,
  PropertyType_Vec3fArray,
  PropertyType_Vec4fArray,
  PropertyType_Mat4fArray,
};

/// Property base class
class Property
{
  public:
    /// Constructors
    template <class T> Property(const char* name, const T& value);

    /// Type of property
    PropertyType type() const;

    /// Get value
    template <class T> T& get();
    template <class T> const T& get() const;

    /// Set value
    template <class T> void set(const T& data);

    /// Get property type string
    static const char* get_type_name(PropertyType type);

  private:
    struct Value;
    template <class T> struct ValueImpl;
    std::shared_ptr<Value> value;
};

/// Property map
class PropertyMap
{
  public:
    /// Constructor
    PropertyMap();

    /// Number of properties
    size_t count() const;

    /// Property list
    const Property* items() const;
    Property* items();

    /// Find property by name
    const Property* find(const char* name) const;
    Property* find(const char* name);

    /// Get property by name or throw an exception
    const Property& get(const char* name) const;
    Property& get(const char* name);

    const Property& operator[](const char* name) const;
    Property& operator[](const char* name);

    /// Add property
    size_t insert(const char* name, const Property& property);

    /// Remove property
    void erase(const char* name);

    /// Remove all properties
    void clear();

    /// Set property
    template <class T> Property& set(const char* name, const T& value);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

#include <common/detail/property_map.inl>

}}
