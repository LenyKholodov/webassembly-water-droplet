#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

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

      visitor.traverse(*root_node);

        //process all mirrors

      for (auto entity : visitor.prerender_entities())
      {
        if (entity->is_environment_map_required())
          prerender_environment_map(entity, context);
      }
    }

    void prerender_environment_map(const Entity::Pointer& entity, ScenePassContext& context)
    {
        //create envmap data

      EnvironmentMap* envmap = EnvironmentMap::get(*entity, context, MIRROR_TEXTURE_SIZE);

      engine_check(envmap != nullptr && "Environment map has not been created");
     
        //initiate sub-renderings for all cubemap faces

      for (std::shared_ptr<Portal>& portal : envmap->portals)
      {
            //configure viewport

        SceneViewport scene_viewport(portal->frame_buffer);        

        //scene_viewport.set_view_node(entity, );
        //todo: setup framebuffer

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
