#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::scene::passes;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

///
/// Constants
///
static constexpr size_t RESERVED_MESHES_COUNT = 1024;
static constexpr size_t RESERVED_POINT_LIGHTS_COUNT = 256;
static constexpr size_t RESERVED_SPOT_LIGHTS_COUNT = 256;
static constexpr size_t RESERVED_PROJECTILES_COUNT = 16;
static constexpr size_t RESERVED_PRERENDERS_COUNT = 16;

/// Scene visitor implementation details
struct SceneVisitor::Impl
{
  const ScenePassOptions* options; //scene pass options
  MeshArray meshes;
  PointLightArray point_lights;
  SpotLightArray spot_lights;
  ProjectileArray projectiles;
  EntityArray prerender_entities;

  Impl()
    : options()
  {
    meshes.reserve(RESERVED_MESHES_COUNT);
    point_lights.reserve(RESERVED_POINT_LIGHTS_COUNT);
    spot_lights.reserve(RESERVED_SPOT_LIGHTS_COUNT);
    projectiles.reserve(RESERVED_PROJECTILES_COUNT);
    prerender_entities.reserve(RESERVED_PRERENDERS_COUNT);
  }

  bool is_excluded(Node& node)
  {
    if (!options || options->excluded_nodes.empty())
      return false;

    return options->excluded_nodes.find(&node) != options->excluded_nodes.end();
  }
};

SceneVisitor::SceneVisitor()
  : impl(std::make_shared<Impl>())
{

}

const MeshArray& SceneVisitor::meshes() const
{
  return impl->meshes;
}

const PointLightArray& SceneVisitor::point_lights() const
{
  return impl->point_lights;
}

const SpotLightArray& SceneVisitor::spot_lights() const
{
  return impl->spot_lights;
}

const ProjectileArray& SceneVisitor::projectiles() const
{
  return impl->projectiles;
}

const EntityArray& SceneVisitor::prerender_entities() const
{
  return impl->prerender_entities;
}

void SceneVisitor::reset()
{
  impl->meshes.clear();
  impl->point_lights.clear();
  impl->spot_lights.clear();
  impl->projectiles.clear();
  impl->prerender_entities.clear();
  impl->options = nullptr;
}

void SceneVisitor::traverse(Node& node, const ScenePassOptions* options)
{
  impl->options = options;

  try
  {
    node.traverse(*this);
  }
  catch(...)
  {
    impl->options = nullptr;
    throw;
  }

  impl->options = nullptr;
}

void SceneVisitor::visit(engine::scene::Mesh& node)
{
  if (impl->is_excluded(node))
    return;

  impl->meshes.push_back(engine::scene::Mesh::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(engine::scene::Entity& node)
{
  if (impl->is_excluded(node))
    return;

  if (node.is_environment_map_required())
    impl->prerender_entities.push_back(engine::scene::Entity::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(SpotLight& node)
{
  if (impl->is_excluded(node))
    return;

  impl->spot_lights.push_back(SpotLight::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(PointLight& node)
{
  if (impl->is_excluded(node))
    return;

  impl->point_lights.push_back(PointLight::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(Projectile& node)
{
  if (impl->is_excluded(node))
    return;

  impl->projectiles.push_back(Projectile::Pointer(node.shared_from_this(), &node));
}
