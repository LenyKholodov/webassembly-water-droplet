#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

namespace render {
namespace scene {
namespace passes {

/// Test scene render pass component
struct TestPass : IScenePass
{
  TestPass(SceneRenderer&)
  {
    engine_log_debug(__FUNCTION__);
  }

  static IScenePass* create(SceneRenderer& renderer, Device&)
  {
    return new TestPass(renderer);
  }

  void get_dependencies(std::vector<std::string>&)
  {
    engine_log_debug(__FUNCTION__);
  }

  void render(ScenePassContext& context)
  {
    engine_log_debug(__FUNCTION__);
  }
};

struct TestPassComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("test_pass", &TestPass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("test_pass"); 
  }
};

static TestPassComponent component;

}}}
