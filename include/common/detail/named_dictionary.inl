template <class Value>
size_t NamedDictionary<Value>::size() const
{
  return storage.size();
}

template <class Value>
Value* NamedDictionary<Value>::find(const StringRef& name)
{
  if (!name.size())
    return nullptr;

  auto range = storage.equal_range(name);

  for (; range.first != range.second; ++range.first)
  {
    auto& value_desc = range.first->second;

    if (name == value_desc.first)
      return &value_desc.second;
  }

  return nullptr;
}

template <class Value>
const Value* NamedDictionary<Value>::find(const StringRef& name) const
{
  return const_cast<NamedDictionary<Value>&>(*this).find(name);
}

template <class Value>
void NamedDictionary<Value>::insert(const StringRef& name, const Value& value)
{
  engine_check(name.size() > 0);

  auto range = storage.equal_range(name);

  for (; range.first != range.second; ++range.first)
  {
    auto& value_desc = range.first->second;

    if (name != value_desc.first)
      continue;

    throw Exception::format("Key '%s' has been already added to a dictionary", name.to_string().c_str());
  }

  storage.insert(std::make_pair(name, std::make_pair(name.to_string(), value)));
}

template <class Value>
void NamedDictionary<Value>::erase(const StringRef& name)
{
  if (!name.size())
    return;

  auto range = storage.equal_range(name);

  for (; range.first != range.second;)
  {
    auto& value_desc = range.first->second;

    if (name != value_desc.first)
    {
      ++range.first;
      continue;
    }

    auto it = range.first++;

    storage.erase(it);
  }
}

template <class Value>
void NamedDictionary<Value>::clear()
{
  storage.clear();
}
