#pragma once

#include <common/exception.h>
#include <common/string.h>

#include <unordered_map>

namespace engine {
namespace common {

/// Named dictionary
template <class Value>
class NamedDictionary
{
  public:
    /// Number of values
    size_t size() const;

    /// Find value by name
    Value* find(const StringRef&);
    const Value* find(const StringRef&) const;

    /// Insert element
    void insert(const StringRef& name, const Value& value);

    /// Erase element
    void erase(const StringRef& name);

    /// Clearing
    void clear();

  private:
    typedef std::unordered_multimap<StringHash, std::pair<std::string, Value>, StringHash::Hasher> Storage;

  private:
    Storage storage;
};

#include <common/detail/named_dictionary.inl>

}}
