///
/// BindingContext
///

template <class T1>
BindingContext::BindingContext(const T1& arg1)
{
  bind(arg1);
}

template <class T1, class T2>
BindingContext::BindingContext(const T1& arg1, const T2& arg2)
{
  bind(arg1);
  bind(arg2);
}

template <class T1, class T2, class T3>
BindingContext::BindingContext(const T1& arg1, const T2& arg2, const T3& arg3)
{
  bind(arg1);
  bind(arg2);
  bind(arg3);
}

template <class T1, class T2, class T3, class T4>
BindingContext::BindingContext(const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4)
{
  bind(arg1);
  bind(arg2);
  bind(arg3);
  bind(arg4);
}

inline void BindingContext::bind(const BindingContext* context)
{
  if (!context)
    return;

  for (size_t i=0; i<sizeof(parent)/sizeof(*parent); i++)
    if (!parent[i])
    {
      parent[i] = context;
      return;
    }

  throw common::Exception::format("Can't link contexts; all parents are bound");
}

inline void BindingContext::unbind(const BindingContext* context)
{
  for (size_t i=0; i<sizeof(parent)/sizeof(*parent); i++)
    if (parent[i] == context)
    {
      parent[i] = nullptr;
    }
}

inline void BindingContext::bind(const TextureList& in_textures)
{
  engine_check(textures == nullptr);

  textures = &in_textures;
}

inline void BindingContext::bind(const PropertyMap& in_properties)
{
  engine_check(properties == nullptr);

  properties = &in_properties;
}

inline void BindingContext::bind(const Material& material)
{
  bind(material.properties());
  bind(material.textures());
}

inline void BindingContext::unbind_all()
{
  *this = BindingContext();
}

template <class T, class Finder>
const T* BindingContext::find(const BindingContext* context, const char* name, Finder fn)
{
  if (!name)
    return nullptr;

  if (!context)
    return nullptr;

  if (const T* value = fn(*context))
    return value;

  for (size_t i=0; i<sizeof(parent)/sizeof(*parent); i++)
  {
    if (const T* value = find<T>(context->parent[i], name, fn))
      return value;
  }
  
  return nullptr;
}

inline const Property* BindingContext::find_property(const char* name) const
{
  auto finder = [&](const BindingContext& context) -> const Property*
  {
    if (!context.properties)
      return nullptr;

    if (const Property* property = context.properties->find(name))
      return property;

    return nullptr;
  };

  return find<Property>(this, name, finder);
}

inline const Texture* BindingContext::find_texture(const char* name) const
{
  auto finder = [&](const BindingContext& context) -> const Texture*
  {
    if (!context.textures)
      return nullptr;

    if (const Texture* texture = context.textures->find(name))
      return texture;

    return nullptr;
  };

  return find<Texture>(this, name, finder);
}
