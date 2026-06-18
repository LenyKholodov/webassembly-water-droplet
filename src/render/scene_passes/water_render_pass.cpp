#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

namespace engine {
namespace render {
namespace scene {
namespace passes {

static constexpr size_t WATER_RT_SIZE = 1024; // reflection/refraction render-target resolution

/// Planar reflection + refraction prerender for flat water surfaces.
/// Renders the scene from a mirrored camera (reflection) and the main camera (refraction)
/// into two textures that the water shader samples in screen space.
class WaterReflectionPass : IScenePass
{
  public:
    WaterReflectionPass(SceneRenderer&)
    {
      engine_log_debug("Water reflection pass has been created");
    }

    static IScenePass* create(SceneRenderer& renderer, Device&)
    {
      return new WaterReflectionPass(renderer);
    }

    void get_dependencies(std::vector<std::string>&)
    {
    }

    void prerender(ScenePassContext& context)
    {
      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

      visitor.traverse(*root_node, &context.options());

        //find the water surface; collect env-map entities (droplets) to exclude from the nested
        //renders so we don't re-trigger their cubemap prerendering (a huge cost cascade)

      Entity::Pointer water;

        //refraction excludes the water (recursion) + env-map droplets (cubemap cascade);
        //reflection additionally excludes below-water geometry that would occlude the mirror camera

      std::shared_ptr<ScenePassOptions> refraction_options = std::make_shared<ScenePassOptions>();
      std::shared_ptr<ScenePassOptions> reflection_options = std::make_shared<ScenePassOptions>();

      for (auto& entity : visitor.prerender_entities())
      {
        if (entity->is_planar_reflection_required())
          water = entity;

        if (entity->is_environment_map_required())
        {
          refraction_options->excluded_nodes.insert(&*entity);
          reflection_options->excluded_nodes.insert(&*entity);
        }
      }

      if (!water)
      {
        visitor.reset();
        return;
      }

      refraction_options->excluded_nodes.insert(&*water); // never refract the water itself (also stops recursion)
      reflection_options->excluded_nodes.insert(&*water); // never reflect the water itself (also stops recursion)

      for (auto& mesh : visitor.meshes())
      {
        if (mesh->is_reflection_excluded())
          reflection_options->excluded_nodes.insert(&*mesh);

          //metaball-raymarch droplets (marked by their per-node particle PropertyMap) sample THIS water
          //pass's refraction texture in screen space. Keep them out of both nested renders: that texture
          //isn't bound inside the nested pass (the water excludes itself), so rendering a droplet there
          //throws "can't find refractionTexture", and it would also be a feedback dependency.
          //In dynamic-cubemap mode they're already excluded above via is_environment_map_required.
        if (mesh->find_user_data<common::PropertyMap>())
        {
          reflection_options->excluded_nodes.insert(&*mesh);
          refraction_options->excluded_nodes.insert(&*mesh);
        }
      }

      WaterReflection* wr = WaterReflection::get(*water, context, WATER_RT_SIZE, WATER_RT_SIZE);

        //main camera + the water plane height

      Node::Pointer  main_camera   = context.view_node();
      math::mat4f    proj          = context.projection_tm();
      math::mat4f    cam_world     = main_camera->world_tm();
      math::mat4f    cam_world_inv = inverse(cam_world);
      float          plane_y       = (water->world_tm() * math::vec3f(0.0f, 0.0f, 0.0f, 1.0f)).y;

        //reflection about the plane y = plane_y :  (x, y, z) -> (x, 2*plane_y - y, z)

      math::mat4f reflect_tm(1.0f);
      reflect_tm[1] = math::vec4f(0.0f, -1.0f, 0.0f, 2.0f * plane_y);

      Viewport      vp(0, 0, (int)WATER_RT_SIZE, (int)WATER_RT_SIZE);
      math::vec4f   clear_color(0.01f, 0.02f, 0.04f, 1.0f);

        //refraction: the scene below the water, from the main camera (subview = identity -> camera_world = cam_world)

      {
        SceneViewport sv(wr->refraction_frame_buffer);
        sv.set_clear_color(clear_color);
        sv.set_view_node(main_camera, proj, math::mat4f(1.0f));
        sv.set_viewport(vp);
        sv.set_options(refraction_options);
        context.renderer().render(sv);
      }

        //reflection: the scene from the mirrored camera (camera_world = reflect_tm * cam_world)

      {
        math::mat4f subview = cam_world_inv * reflect_tm * cam_world;

        SceneViewport sv(wr->reflection_frame_buffer);
        sv.set_clear_color(clear_color);
        sv.set_view_node(main_camera, proj, subview);
        sv.set_viewport(vp);
        sv.set_options(reflection_options);
        context.renderer().render(sv);
      }

      visitor.reset();
    }

    void render(ScenePassContext&)
    {
    }

  private:
    SceneVisitor visitor;
};

struct WaterReflectionPassComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("Water Reflection", &WaterReflectionPass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("Water Reflection");
  }
};

static WaterReflectionPassComponent component;

}}}}
