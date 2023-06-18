#pragma once

#include <scene/camera.h>
#include <scene/node.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <scene/projectile.h>
#include <render/scene_render.h>
#include <media/geometry.h>

/// Game world
class World
{
  public:
    /// Constructor
    World(engine::scene::Node::Pointer root_node, engine::render::scene::SceneRenderer& scene_render);

    /// Destructor
    ~World();

    /// Update world
    void update();

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};
