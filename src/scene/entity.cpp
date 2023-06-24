#include <scene/mesh.h>

using namespace engine::scene;

/// Entity implementation details
struct Entity::Impl
{
  bool require_environment_map = false;
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

void Entity::visit(ISceneVisitor& visitor)
{
  Node::visit(visitor);

  visitor.visit(*this);
}
