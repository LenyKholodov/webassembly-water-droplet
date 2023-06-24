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
    World(engine::scene::Node::Pointer root_node, engine::render::scene::SceneRenderer& scene_render, const engine::scene::Camera::Pointer& camera);

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


enum class SoundId
{
  droplet_ground,
  droplet_leaf
};

/// Sound player
class SoundPlayer
{
  public:
    /// Constructor
    SoundPlayer();

    /// Play music
    void play_music(bool force = false) const;
    
    /// Play sound
    static void play_sound(SoundId sound_id, float volume = 1.f);

    /// Update
    void update();

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};
