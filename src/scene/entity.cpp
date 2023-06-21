#include <scene/mesh.h>

using namespace engine::scene;

/// Entity implementation details
struct Entity::Impl
{
  bool require_environment_map = false;
};

Entity::Entity()
  : impl(std::make_unique<Impl>())
{
}

Entity::~Entity()
{
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
