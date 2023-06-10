#include "shared.h"

using namespace engine::render;
using namespace engine::render::low_level;
using namespace engine::render::scene;
using namespace engine::render::scene::passes;
using namespace engine::scene;
using namespace engine::common;

namespace engine {
namespace render {
namespace scene {
namespace passes {

///
/// Constants
///

static const char* PROJECTILE_PROGRAM_FILE = "media/shaders/projectile.glsl";

/// Projectile map rendering pass
class ProjectilePass : IScenePass
{
  public:
    ProjectilePass(SceneRenderer& renderer)
      : projectile_program(renderer.device().create_program_from_file(PROJECTILE_PROGRAM_FILE))
      , shared_textures(renderer.textures())
      , projectile_frame_buffer(renderer.device().create_frame_buffer())
      , projectile_pass(renderer.device().create_pass(projectile_program))
    {
      projectile_pass.set_blend_state(BlendState(true, BlendArgument_One, BlendArgument_One));
      projectile_pass.set_depth_stencil_state(DepthStencilState(false, false, CompareMode_AlwaysPass));
      projectile_pass.set_clear_flags(Clear_None);
      projectile_pass.set_frame_buffer(projectile_frame_buffer);

      engine_log_debug("Projectile pass has been created");
    }

    static IScenePass* create(SceneRenderer& renderer, Device&)
    {
      return new ProjectilePass(renderer);
    }

    void get_dependencies(std::vector<std::string>& deps)
    {
      deps.push_back("G-Buffer");
      deps.push_back("Shadow Maps Rendering");
    }

    void render(ScenePassContext& context)
    {
        //search for G-Buffer frame and add it to dependency list

      if (!g_buffer_frame_initialized)
      {
        g_buffer_frame = context.frame_nodes().get("g_buffer");

        Texture normal_texture = shared_textures.get("normalTexture");
        Texture albedo_texture = shared_textures.get("albedoTexture");

        projectile_frame_buffer.attach_color_target(albedo_texture);
        projectile_frame_buffer.attach_color_target(normal_texture);
        projectile_frame_buffer.reset_viewport();

        g_buffer_frame_initialized = true;
      }

      frame.add_dependency(g_buffer_frame);

        //traverse scene

      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

        //traverse scene

      visitor.traverse(*root_node);

        //configure view

      PropertyMap pass_properties = projectile_pass.properties();

      pass_properties.set("viewMatrix", math::mat4f(1.0f));
      pass_properties.set("worldViewPosition", math::vec3f(0.0f));
      pass_properties.set("projectionMatrix", math::mat4f(1.0f));

        //render projectiles

      for (auto& projectile : visitor.projectiles())
      {
        render_projectile_map(projectile, context);
      }

        //add projectile pass to Projectile frame

      frame.add_pass(projectile_pass);      
      frame.add_dependency(g_buffer_frame);

      context.root_frame_node().add_dependency(frame);      

        //clear data

      visitor.reset();
    }

  private:
    void render_projectile_map(const Projectile::Pointer& projectile, ScenePassContext& context)
    {
        //TODO: render only part of projectile (scissor & projectile volume)
        //create projectile data

      Shadow* shadow = projectile->find_user_data<Shadow>();

      engine_check(shadow != nullptr);

      RenderableProjectile* renderable_projectile = projectile->find_user_data<RenderableProjectile>();

      if (!renderable_projectile)
      {
        renderable_projectile = &projectile->set_user_data(RenderableProjectile(projectile->image(), shadow->shadow_texture, context.device()));
      }

        //configure properties

      PropertyMap projectile_properties = renderable_projectile->properties;

      projectile_properties.set("shadowMatrix", shadow->shadow_tm);
      projectile_properties.set("projectileColor", projectile->color() * projectile->intensity());

        //render projectile

      projectile_pass.add_primitive(renderable_projectile->plane, math::mat4f(1.0f), projectile_properties);
    }

  private:
    low_level::Program projectile_program;
    TextureList shared_textures;
    FrameBuffer projectile_frame_buffer;
    Pass projectile_pass;
    FrameNode frame;
    SceneVisitor visitor;
    bool g_buffer_frame_initialized = false;
    FrameNode g_buffer_frame;
};

struct ProjectilePassComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("Projectile Maps Rendering", &ProjectilePass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("Projectile Maps Rendering"); 
  }
};

static ProjectilePassComponent component;

}}}}
