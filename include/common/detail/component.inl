inline ComponentScope::ComponentScope(const char* name_wildcard)
  : name_wildcard(name_wildcard)
{
  Component::enable(name_wildcard);
}

inline ComponentScope::~ComponentScope()
{
  Component::disable(name_wildcard);
}
