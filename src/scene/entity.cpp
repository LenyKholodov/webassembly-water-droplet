#include <scene/mesh.h>

using namespace engine::scene;

/// Entity implementation details
struct Entity::Impl
{
  bool require_environment_map = false;
  bool require_planar_reflection = false;
  bool reflection_excluded = false;
  math::vec3f environment_map_local_point;
};

Entity::Entity()
  : impl(std::make_unique<Impl>())
{
}

Entity::~Entity()
{
}

const math::vec3f& Entity::environment_map_local_point() const
{
  return impl->environment_map_local_point;
}

void Entity::set_environment_map_local_point(const math::vec3f& point)
{
  impl->environment_map_local_point = point;
}

bool Entity::is_environment_map_required() const
{
  return impl->require_environment_map;
}

void Entity::set_environment_map_required(bool state)
{
  impl->require_environment_map = state;
}

bool Entity::is_planar_reflection_required() const
{
  return impl->require_planar_reflection;
}

void Entity::set_planar_reflection_required(bool state)
{
  impl->require_planar_reflection = state;
}

bool Entity::is_reflection_excluded() const
{
  return impl->reflection_excluded;
}

void Entity::set_reflection_excluded(bool state)
{
  impl->reflection_excluded = state;
}

void Entity::visit(ISceneVisitor& visitor)
{
  Node::visit(visitor);

  visitor.visit(*this);
}
