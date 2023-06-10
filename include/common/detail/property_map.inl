/// Property type mapping
template <class T> struct PropertyTypeMap;

template <> struct PropertyTypeMap<int>         { static constexpr PropertyType type = PropertyType_Int; };
template <> struct PropertyTypeMap<float>       { static constexpr PropertyType type = PropertyType_Float; };
template <> struct PropertyTypeMap<math::vec2f> { static constexpr PropertyType type = PropertyType_Vec2f; };
template <> struct PropertyTypeMap<math::vec3f> { static constexpr PropertyType type = PropertyType_Vec3f; };
template <> struct PropertyTypeMap<math::vec4f> { static constexpr PropertyType type = PropertyType_Vec4f; };
template <> struct PropertyTypeMap<math::mat4f> { static constexpr PropertyType type = PropertyType_Mat4f; };

template <> struct PropertyTypeMap<std::vector<int>>         { static constexpr PropertyType type = PropertyType_IntArray; };
template <> struct PropertyTypeMap<std::vector<float>>       { static constexpr PropertyType type = PropertyType_FloatArray; };
template <> struct PropertyTypeMap<std::vector<math::vec2f>> { static constexpr PropertyType type = PropertyType_Vec2fArray; };
template <> struct PropertyTypeMap<std::vector<math::vec3f>> { static constexpr PropertyType type = PropertyType_Vec3fArray; };
template <> struct PropertyTypeMap<std::vector<math::vec4f>> { static constexpr PropertyType type = PropertyType_Vec4fArray; };
template <> struct PropertyTypeMap<std::vector<math::mat4f>> { static constexpr PropertyType type = PropertyType_Mat4fArray; };

/// Property value
struct Property::Value
{
  const PropertyType type;
  std::string name;

  Value(std::string&& name, PropertyType type) : type(type), name(name) {}

  virtual ~Value() = default;
};

/// Property value implementation
template <class T> struct Property::ValueImpl: public Value
{
  T data;

  ValueImpl(std::string&& name, const T& data)
    : Value(std::move(name), PropertyTypeMap<T>::type)
    , data(data)
  {
  }
};

template <class T>
Property::Property(const char* name, const T& data)
{
  engine_check_null(name);

  std::string name_string = name;

  value = std::make_shared<ValueImpl<T>>(std::move(name_string), data);
}

inline PropertyType Property::type() const
{
  return value->type;
}

inline const char* Property::get_type_name(PropertyType type)
{
  switch (type)
  {
    case PropertyType_Int:        return "int";
    case PropertyType_Float:      return "float";
    case PropertyType_Vec2f:      return "vec2f";
    case PropertyType_Vec3f:      return "vec3f";
    case PropertyType_Vec4f:      return "vec4f";
    case PropertyType_Mat4f:      return "mat4f";
    case PropertyType_IntArray:   return "int[]";
    case PropertyType_FloatArray: return "float[]";
    case PropertyType_Vec2fArray: return "vec2f[]";
    case PropertyType_Vec3fArray: return "vec3f[]";
    case PropertyType_Vec4fArray: return "vec4f[]";
    case PropertyType_Mat4fArray: return "mat4f[]";
    default:                      return "<unknown>";
  }
}

template <class T>
T& Property::get()
{
  constexpr PropertyType expected_type = PropertyTypeMap<T>::type;

  if (expected_type == value->type)
    return static_cast<ValueImpl<T>*>(value.get())->data;

  throw Exception::format("PropertyType mismatch: requested %s, actual %s",
    get_type_name(expected_type), get_type_name(value->type));
}

template <class T>
const T& Property::get() const
{
  return const_cast<Property&>(*this).get<T>();
}

template <class T>
void Property::set(const T& data)
{
  constexpr PropertyType expected_type = PropertyTypeMap<T>::type;

  if (expected_type == value->type)
  {
    static_cast<ValueImpl<T>*>(value.get())->data = data;
    return;
  }

  std::shared_ptr<Value> new_value(new ValueImpl<T>(std::move(value->name), data));

  value = new_value;
}

inline const Property& PropertyMap::operator[](const char* name) const
{
  return get(name);
}

inline Property& PropertyMap::operator[](const char* name)
{
  return get(name);
}

template <class T>
Property& PropertyMap::set(const char* name, const T& value)
{
  if (Property* property = find(name))
  {
    property->set(value);
    return *property;
  }

  Property property(name, value);

  size_t index = insert(name, property);

  return items()[index];
}
