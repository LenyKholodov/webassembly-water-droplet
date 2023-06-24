///
/// StringRef
///

inline StringRef::StringRef(const char* string)
  : start(string)
  , end(start)
{
  if (end)
    end += strlen(string);
}

inline StringRef::StringRef(const std::string& string)
  : start(string.c_str())
  , end(start + string.size())
{
}

inline std::string StringRef::to_string() const
{
  return std::string(start, end);
}

inline bool StringRef::operator == (const StringRef& other) const
{
  return size() == other.size() && std::equal(start, end, other.start);
}

inline bool StringRef::operator != (const StringRef& other) const
{
  return !(*this == other);
}

///
/// StringHash
///

inline StringHash::StringHash(const StringRef& string)
  : hash(compute_hash(string.data(), string.size()))
{
}

inline size_t StringHash::compute_hash(const char* s, size_t len)
{
  size_t hash = 0;

  while (len--) hash = 5 * hash + *s++;

  return hash;
}

inline bool StringHash::operator == (const StringHash& other) const
{
  return hash == other.hash;
}

inline bool StringHash::operator != (const StringHash& other) const
{
  return !(*this == other);
}
