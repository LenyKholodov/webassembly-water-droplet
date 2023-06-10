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

/// Scene visitor implementation details
struct SceneVisitor::Impl
{
  MeshArray meshes;
  PointLightArray point_lights;
  SpotLightArray spot_lights;
  ProjectileArray projectiles;

  Impl()
  {
    meshes.reserve(RESERVED_MESHES_COUNT);
    point_lights.reserve(RESERVED_POINT_LIGHTS_COUNT);
    spot_lights.reserve(RESERVED_SPOT_LIGHTS_COUNT);
    projectiles.reserve(RESERVED_PROJECTILES_COUNT);
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

void SceneVisitor::reset()
{
  impl->meshes.clear();
  impl->point_lights.clear();
  impl->spot_lights.clear();
  impl->projectiles.clear();
}

void SceneVisitor::traverse(Node& node)
{
  node.traverse(*this);
}

void SceneVisitor::visit(engine::scene::Mesh& node)
{
  impl->meshes.push_back(engine::scene::Mesh::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(SpotLight& node)
{
  impl->spot_lights.push_back(SpotLight::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(PointLight& node)
{
  impl->point_lights.push_back(PointLight::Pointer(node.shared_from_this(), &node));
}

void SceneVisitor::visit(Projectile& node)
{
  impl->projectiles.push_back(Projectile::Pointer(node.shared_from_this(), &node));
}
