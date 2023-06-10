#include <common/component.h>
#include <common/exception.h>
#include <common/string.h>
#include <common/log.h>

#include <cxxabi.h>

using namespace engine::common;

static Component* first = nullptr;

Component::Component()
  : enabled()
{
  Component* last = first;

  for (;last && last->next; last=last->next);

  prev = last;
  next = nullptr;

  if (prev) prev->next = this;
  else      first = this;
}

Component::~Component()
{
  if (prev) prev->next = next;
  else      first = next;

  if (next) next->prev = prev;
}

const char* Component::name() const
{
  const char* name = typeid(*this).name();

  int status = 0;
  const char* demangled_name = abi::__cxa_demangle(name, 0, 0, &status);

  if (!demangled_name)
    return name;

  return demangled_name;
}

void Component::enable()
{
  if (enabled++)
    return;

  engine_log_info("...loading component '%s'...", name());  

  load();
}

void Component::disable()
{
  if (--enabled)
    return;

  engine_log_info("...unloading component '%s'...", name());

  unload();
}

namespace
{


}

void Component::enable(const char* name_wildcard)
{
  engine_check_null(name_wildcard);

  engine_log_info("Enabling components '%s':", name_wildcard);

  for (Component* component=first; component; component=component->next)
  {
    if (!wcmatch(component->name(), name_wildcard))
      continue;

    component->enable();
  }
}

void Component::disable(const char* name_wildcard)
{
  if (!name_wildcard)
    return;

  engine_log_info("Disabling components '%s':", name_wildcard);  

  for (Component* component=first; component; component=component->next)
  {
    if (!wcmatch(component->name(), name_wildcard))
      continue;

    component->disable();
  }
}
