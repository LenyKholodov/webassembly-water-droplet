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

    /// Input control
    void inputGrab(float ray_start_x, float ray_start_y, float ray_start_z, float ray_end_x, float ray_end_y, float ray_end_z);
    void inputDrag(float target_offset_x, float target_offset_y, float target_offset_z);
    void inputRelease();

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};
