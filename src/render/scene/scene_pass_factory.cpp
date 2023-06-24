#include "shared.h"

using namespace engine::render::scene;
using namespace engine::common;

namespace
{

typedef NamedDictionary<ScenePassFactory::ScenePassCreator> ScenePassCreatorMap;

/// Implementation of scene pass factory
struct ScenePassFactoryImpl
{
  ScenePassCreatorMap creators; //make-functions for scene passes

  /// Singleton instance
  static ScenePassFactoryImpl& instance()
  {
    static ScenePassFactoryImpl instance;
    return instance;
  }
};

}

void ScenePassFactory::register_scene_pass(const char* pass, const ScenePassCreator& create_fn)
{
  engine_check_null(pass);

  ScenePassFactoryImpl& impl = ScenePassFactoryImpl::instance();

  impl.creators.insert(pass, create_fn);

  engine_log_info("Scene rendering pass '%s' has been registered", pass);
}

void ScenePassFactory::unregister_scene_pass(const char* pass)
{
  if (!pass)
    return;

  try
  {
    ScenePassFactoryImpl& impl = ScenePassFactoryImpl::instance();

    impl.creators.erase(pass);

    engine_log_info("Scene rendering pass '%s' has been unregistered", pass);
  }
  catch (...)
  {
    //ignore all exceptions in closing like functions
  }
}

ScenePassPtr ScenePassFactory::create_pass(const char* pass, SceneRenderer& renderer, low_level::Device& device)
{
  engine_check_null(pass);

  ScenePassFactoryImpl& impl = ScenePassFactoryImpl::instance();  

  ScenePassCreator* creator = impl.creators.find(pass);

  if (!creator)
    throw Exception::format("Pass '%s' has not been registered", pass);

  return std::shared_ptr<IScenePass>((*creator)(renderer, device));
}
