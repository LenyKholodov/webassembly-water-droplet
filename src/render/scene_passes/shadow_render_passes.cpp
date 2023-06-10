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

static const size_t SHADOW_MAP_SIZE = 1024;
static const char* SHADOW_PROGRAM_FILE = "media/shaders/shadow.glsl";

/// Shadow map rendering pass
class ShadowPass : IScenePass
{
  public:
    ShadowPass(SceneRenderer& renderer)
      : shadow_program(renderer.device().create_program_from_file(SHADOW_PROGRAM_FILE))
    {
    }

    static IScenePass* create(SceneRenderer& renderer, Device&)
    {
      return new ShadowPass(renderer);
    }

    void get_dependencies(std::vector<std::string>&)
    {
    }

    void render(ScenePassContext& context)
    {
        //traverse scene

      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

        //traverse scene

      visitor.traverse(*root_node);

        //enumerate spot lights and build shadows for them

      for (auto& light : visitor.spot_lights())
      {
        render_shadow_map(light, context);
      }

        //enumerate projectiles and build shadows for them

      for (auto& projectile : visitor.projectiles())
      {
        render_shadow_map(projectile, context);
      }

        //clear data

      visitor.reset();
    }

  private:
    void render_shadow_map(const SpotLight::Pointer& light, ScenePassContext& context)
    {
      render_shadow_map(static_cast<Node&>(*light), light->projection_matrix(), context);
    }

    void render_shadow_map(const Projectile::Pointer& projectile, ScenePassContext& context)
    {
      render_shadow_map(static_cast<Node&>(*projectile), projectile->projection_matrix(), context);
    }    

    void render_shadow_map(Node& node, const math::mat4f& projection_tm, ScenePassContext& context)
    {
        //create shadow data

      Shadow* shadow = node.find_user_data<Shadow>();

      if (!shadow)
      {
        shadow = &node.set_user_data(Shadow(context.device(), shadow_program, SHADOW_MAP_SIZE));
      }

        //configure view

      PropertyMap pass_properties = shadow->shadow_pass.properties();

      math::mat4f view_tm = inverse(node.world_tm());
      math::mat4f view_projection_tm = projection_tm * view_tm;
      math::vec3f world_view_position = node.world_tm() * math::vec3f(0.0f);

      pass_properties.set("viewMatrix", view_tm);
      pass_properties.set("worldViewPosition", world_view_position);
      pass_properties.set("projectionMatrix", projection_tm);

        //update shadow matrix

      shadow->shadow_tm = view_projection_tm;

        //draw geometry

      for (auto& mesh : visitor.meshes())
      {
        render_mesh(*mesh, context, shadow->shadow_pass);
      }

        //add shadow pass to shadow frame

      shadow->shadow_frame.add_pass(shadow->shadow_pass);
    }

    void render_mesh(engine::scene::Mesh& mesh, ScenePassContext& context, Pass& shadow_pass)
    {
        //create mesh data

      RenderableMesh* renderable_mesh = mesh.find_user_data<RenderableMesh>();

      if (!renderable_mesh)
      {
        renderable_mesh = &mesh.set_user_data(RenderableMesh(mesh, context));
      }

        //add mesh to pass

      shadow_pass.add_mesh(renderable_mesh->mesh, mesh.world_tm());
    }

  private:
    low_level::Program shadow_program;
    SceneVisitor visitor;
};

struct ShadowPassComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("Shadow Maps Rendering", &ShadowPass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("Shadow Maps Rendering"); 
  }
};

static ShadowPassComponent component;

}}}}
