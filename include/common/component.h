#pragma once

#include <memory>
#include <functional>
#include <typeinfo>

namespace engine {
namespace common {

/// Engine basic component (for lazy DIP components registration)
class Component
{
  public:
    /// Constructor
    Component();

    /// Destructor
    ~Component();

    /// Name
    const char* name() const;

    /// Register component
    void enable();

    /// Disable component
    void disable();

    /// Enable components
    static void enable(const char* name_wildcard);

    /// Disable components
    static void disable(const char* name_wildcard);

  private:
   virtual void load() = 0;
   virtual void unload() = 0;

  private:
    Component* prev;
    Component* next;
    size_t enabled;
};

/// Scope for components enabling
struct ComponentScope
{
  public:
    /// Constructor
    ComponentScope(const char* name_wildcard);

    /// Destructor
    ~ComponentScope();

  private:
    const char* name_wildcard;
};

#include <common/detail/component.inl>

}}
