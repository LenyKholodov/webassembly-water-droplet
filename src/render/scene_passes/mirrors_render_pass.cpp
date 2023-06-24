#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

static constexpr float ENV_MAP_Z_NEAR = 0.001f;
static constexpr float ENV_MAP_Z_FAR = 100.0f;

namespace engine {
namespace render {
namespace scene {
namespace passes {

/// Constants

static constexpr size_t MIRROR_TEXTURE_SIZE = 512; //mirrors texture size

/// Test scene render pass component
class MirrorsPrerenderPass : IScenePass
{
  public:
    MirrorsPrerenderPass(SceneRenderer&)
    {
      engine_log_debug("Mirrors pass has been created");
    }

    static IScenePass* create(SceneRenderer& renderer, Device&)
    {
      return new MirrorsPrerenderPass(renderer);
    }

    void get_dependencies(std::vector<std::string>&)
    {
    }

    void prerender(ScenePassContext& context)
    {
      //traverse scene

      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

        //traverse scene

      visitor.traverse(*root_node, &context.options());

        //process all mirrors

      for (auto entity : visitor.prerender_entities())
      {
        if (entity->is_environment_map_required())
          prerender_environment_map(entity, context);
      }

        //clear data

      visitor.reset();      
    }

    void prerender_environment_map(const Entity::Pointer& entity, ScenePassContext& context)
    {
        //create envmap data

      EnvironmentMap* envmap = EnvironmentMap::get(*entity, context, MIRROR_TEXTURE_SIZE);

      engine_check(envmap != nullptr && "Environment map has not been created");
     
        //initiate sub-renderings for all cubemap faces

      struct RenderDesc
      {
        math::vec4f color;
        math::vec3f dir;
        math::vec3f up;
        bool        right_hand;
      };

      static constexpr size_t CUBEMAP_FACES_COUNT = 6;

      /*static RenderDesc envmap_descs[CUBEMAP_FACES_COUNT] = {
        {math::vec4f(1, 0, 0, 1), math::vec3f(1, 0, 0),  math::vec3f(0, -1, 0), false},
        {math::vec4f(1, 1, 0, 1), math::vec3f(-1, 0, 0), math::vec3f(0, -1, 0), true},

        {math::vec4f(0, 1, 0, 1), math::vec3f(0, 1, 0),  math::vec3f(0, 0, 1), false},
        {math::vec4f(0, 1, 1, 1), math::vec3f(0, -1, 0), math::vec3f(0, 0, -1), false},

        {math::vec4f(0, 0, 1, 1), math::vec3f(0, 0, 1),  math::vec3f(0, -1, 0), false},
        {math::vec4f(0, 0, 0, 1), math::vec3f(0, 0, -1), math::vec3f(0, -1, 0), false},
      };*/


      static RenderDesc envmap_descs[CUBEMAP_FACES_COUNT] = {
        {math::vec4f(1, 0, 0, 1), math::vec3f(1, 0, 0),  math::vec3f(0, 1, 0), false},
        {math::vec4f(1, 1, 0, 1), math::vec3f(-1, 0, 0), math::vec3f(0, -1, 0), true},

        {math::vec4f(0, 1, 0, 1), math::vec3f(0, 1, 0),  math::vec3f(0, 0, 1), true},
        {math::vec4f(0, 1, 1, 1), math::vec3f(0, -1, 0), math::vec3f(0, 0, 1), false},

        {math::vec4f(0, 0, 1, 1), math::vec3f(0, 0, 1),  math::vec3f(0, 1, 0), false},
        {math::vec4f(0, 0, 0, 1), math::vec3f(0, 0, -1), math::vec3f(0, 1, 0), false},
      };

      size_t map_index = 0;

      for (std::shared_ptr<Portal>& portal : envmap->portals)
      {
        engine_check(map_index < CUBEMAP_FACES_COUNT && "Invalid cubemap face index");

            //get render desc

        const RenderDesc& desc = envmap_descs[map_index];

            //configure viewport

        math::anglef fov = math::degree(90.0f);
        math::mat4f proj_tm = compute_perspective_proj_tm(fov, fov, ENV_MAP_Z_NEAR, ENV_MAP_Z_FAR);
        math::mat4f subview_tm;

        math::vec3f z = normalize(desc.dir), 
                    y = desc.up, 
                    x = normalize(desc.right_hand ? cross(y, z) : cross(z, y));

        y = normalize(cross(z, x));
        
        subview_tm[0] = math::vec4f(x, 0.0f);
        subview_tm[1] = math::vec4f(y, 0.0f);
        subview_tm[2] = math::vec4f(z, 0.0f);
        subview_tm[3] = math::vec4f(0.0f, 0.0f, 0.0f, 1.0f);
        subview_tm    = inverse(subview_tm);
        subview_tm    = math::translate(entity->environment_map_local_point()) * subview_tm;

            //create viewport

        SceneViewport scene_viewport(portal->frame_buffer);
        //SceneViewport scene_viewport = context.renderer().create_window_viewport();;

        //scene_viewport.set_clear_color(desc.color);
        scene_viewport.set_clear_color(math::vec4f(0, 0, 0, 1));
        scene_viewport.set_view_node(entity, proj_tm, subview_tm);

        scene_viewport.set_viewport(Viewport(0, 0, MIRROR_TEXTURE_SIZE, MIRROR_TEXTURE_SIZE));

          //nested render

        std::shared_ptr<ScenePassOptions> options = std::make_shared<ScenePassOptions>();

        options->excluded_nodes.insert(&*entity);

        scene_viewport.set_options(options);

        context.renderer().render(scene_viewport);

          //iteration

        map_index++;
      }
    }

    void render(ScenePassContext& context)
    {
    }

  private:
    SceneVisitor visitor;
};

struct MirrorsPrerenderPassComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("Mirrors", &MirrorsPrerenderPass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("Mirrors"); 
  }
};

static MirrorsPrerenderPassComponent component;

}}}}
