#include "shared.h"

using namespace engine::scene;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::common;

///
/// Constants
///

const size_t RESERVED_PASSES_COUNT = 16; //number of reserved passes per scene renderer

///
/// SceneViewport
///

/// Implementation details of scene viewport
struct SceneViewport::Impl
{
  Camera::Pointer camera; //camera of the scene viewport
  Viewport viewport; //viewport of the scene viewport
  PropertyMap properties; //viewport properties;
  TextureList textures; //viewport textures;
};

SceneViewport::SceneViewport()
  : impl(std::make_shared<Impl>())
{
}

const Viewport& SceneViewport::viewport() const
{
  return impl->viewport;
}

Viewport& SceneViewport::viewport()
{
  return impl->viewport;
}

void SceneViewport::set_viewport(const low_level::Viewport& viewport)
{
  impl->viewport = viewport;
}

Camera::Pointer& SceneViewport::camera() const
{
  return impl->camera;
}

void SceneViewport::set_camera(const scene::Camera::Pointer& camera)
{
  impl->camera = camera;
}

PropertyMap& SceneViewport::properties() const
{
  return impl->properties;
}

void SceneViewport::set_properties(const common::PropertyMap& properties)
{
  impl->properties = properties;
}

TextureList& SceneViewport::textures() const
{
  return impl->textures;
}

void SceneViewport::set_textures(const low_level::TextureList& textures)
{
  impl->textures = textures;
}

///
/// SceneRenderer
///

namespace
{

struct PassEntry;
typedef std::shared_ptr<PassEntry> PassEntryPtr;
typedef std::vector<PassEntryPtr> PassArray;

struct PassEntry
{
  ScenePassPtr pass; //scene pass
  std::string name; //self pass name
  int priority; //priority of pass rendering
  PassArray dependencies; //dependent scene passes
  FrameId rendered_frame_id; //rendered frame ID

  PassEntry(const char* name, const ScenePassPtr& pass, int priority)
    : pass(pass)
    , name(name)
    , priority(priority)
    , rendered_frame_id()
  {
  }

  bool operator < (const PassEntry& other) const { return priority < other.priority; }
};

struct ScenePassContextImpl: ScenePassContext
{
  ScenePassContextImpl(ISceneRenderer& owner)
    : ScenePassContext(owner)
  {
  }

  using ScenePassContext::bind;
  using ScenePassContext::unbind;
};

}

/// Implementation details of scene renderer
struct SceneRenderer::Impl : ISceneRenderer
{
  Device render_device; //rendering device
  TextureList shared_textures; //shared textures
  MaterialList shared_materials; //shared materials
  FrameNodeList shared_frame_nodes; //shared frame_nodes
  common::PropertyMap shared_properties; //shared propertiess
  ScenePassContextImpl passes_context; //scene rendering context
  PassArray passes; //scene rendering passes

  Impl(const Device& device)
    : render_device(device)
    , passes_context(*this)
  {
    passes.reserve(RESERVED_PASSES_COUNT);
  }

  void render_pass(PassEntryPtr& pass_entry)
  {
    FrameId current_frame_id = passes_context.current_frame_id();

    if (pass_entry->rendered_frame_id >= current_frame_id)
      return;

      //render dependencies

    for (auto& dep_pass_entry : pass_entry->dependencies)
    {
      render_pass(dep_pass_entry);
    }

      //render pass

    pass_entry->pass->render(passes_context);

      //update frame info

    pass_entry->rendered_frame_id = current_frame_id;
  }

  PropertyMap& properties() override { return shared_properties; }
  TextureList& textures() override { return shared_textures; }
  MaterialList& materials() override { return shared_materials; }
  FrameNodeList& frame_nodes() override { return shared_frame_nodes; } 
  Device& device() override { return render_device; }
};

SceneRenderer::SceneRenderer(const Window& window, const DeviceOptions& options)
{
  Device device(window, options);

  impl = std::make_shared<Impl>(device);
}

Device& SceneRenderer::device() const
{
  return impl->render_device;
}

size_t SceneRenderer::passes_count() const
{
  return impl->passes.size();
}

namespace
{

struct PassResolver
{
  SceneRenderer& renderer;
  Device& device;
  int priority;
  std::string root_pass;
  PassArray passes;
  const PassArray& existing_passes;

  PassResolver(SceneRenderer& renderer, const PassArray& existing_passes, Device& device, const char* root_pass, int priority)
    : renderer(renderer)
    , device(device)
    , priority(priority)
    , root_pass(root_pass)
    , existing_passes(existing_passes)
  {
    passes.reserve(RESERVED_PASSES_COUNT);
  }

  struct StackFrame
  {
    PassEntryPtr pass;
    const char* name;    
    StackFrame* prev;

    StackFrame(const char* name, const PassEntryPtr& pass, StackFrame* prev)
      : pass(pass)
      , name(name)
      , prev(prev)
    {
    }
  };

  static PassEntryPtr find_pass(const char* name, const PassArray& passes)
  {
    for (const auto& pass_entry : passes)
    {
      if (pass_entry->name == name)
        return pass_entry;
    }

    return nullptr;
  }

  static bool check_loop(const char* name, StackFrame* parent)
  {
    for (StackFrame* it=parent; it; it=it->prev)
    {
      if (!strcmp(it->name, name))
        return true;
    }

    return false;
  }

  static void sort(PassArray& passes)
  {
    std::stable_sort(passes.begin(), passes.end(), [](const auto& ptr1, const auto& ptr2) { return *ptr1 < *ptr2; });
  }

  PassEntryPtr create_pass(const char* name, StackFrame* parent)
  {
      //check loops

    if (check_loop(name, parent))
    {
      std::string stack = name;

      for (StackFrame* it=parent; it; it=it->prev)
      {
        stack = engine::common::format("%s -> %s", it->name, stack.c_str());
      }

      throw Exception::format("Can't create pass '%s' due to pass depenency loop: %s", root_pass.c_str(), stack.c_str());
    }

      //create pass

    ScenePassPtr pass = ScenePassFactory::create_pass(name, renderer, device);
    PassEntryPtr entry = std::make_shared<PassEntry>(name, pass, priority);
    StackFrame frame(name, entry, parent);    

      //create dependencies

    std::vector<std::string> deps;

    pass->get_dependencies(deps);

    for (auto& dep : deps)
    {
      add_pass(dep.c_str(), &frame);
    }

    passes.push_back(entry);

      //add pass to parent

    if (parent && parent->pass)
    {
      parent->pass->dependencies.push_back(entry);

      //TODO: optimize sorting

      sort(parent->pass->dependencies);
    }

    return entry;
  }

  PassEntryPtr add_pass(const char* name, StackFrame* parent)
  {
    if (!parent)
    {
      engine_log_debug("Resolving scene pass '%s'", name);
    }
    else
    {
      engine_log_debug("...creating scene pass '%s' for '%s'", name, root_pass.c_str());
    }

      //check if we have already created such pass

    PassEntryPtr pass = find_pass(name, existing_passes);

    if (!pass)
    {
      pass = find_pass(name, passes);
    }

      //create new pass

    if (!pass)
    {
      pass = create_pass(name, parent);
    }

    return pass;
  }
};

}

void SceneRenderer::add_pass(const char* name, int priority)
{
  engine_check_null(name);

    //create pass

  PassResolver resolver(*this, impl->passes, impl->render_device, name, priority);

  resolver.add_pass(name, nullptr);

    //register pass

  impl->passes.insert(impl->passes.end(), resolver.passes.begin(), resolver.passes.end());

  PassResolver::sort(impl->passes);
}

void SceneRenderer::remove_pass(const char* name)
{
  if (!name)
    return;

  impl->passes.erase(std::remove_if(impl->passes.begin(), impl->passes.end(), [&](const auto& pass_entry) {
    return pass_entry->name == name;
  }), impl->passes.end());
}

void SceneRenderer::render(const SceneViewport& viewport)
{
  render(1, &viewport);
}

void SceneRenderer::render(size_t viewports_count, const SceneViewport* viewports)
{
  if (viewports_count)
    engine_check_null(viewports);

    //update frame info

  impl->passes_context.set_current_frame_id(impl->passes_context.current_frame_id() + 1);

    //render passes

  struct ViewportContextBindings
  {
    BindingContext bindings;
    ScenePassContextImpl& context;

    ViewportContextBindings(ScenePassContextImpl& context)
      : context(context)
    {
      context.bind(&bindings);
    }

    ~ViewportContextBindings()
    {
      context.unbind(&bindings);
    }
  };

  BindingContext renderer_bindings(impl->shared_textures, impl->shared_properties);
  ScenePassContextImpl& context = impl->passes_context;
  Device& device = impl->render_device;
  FrameBuffer& window_frame_buffer = device.window_frame_buffer();

  for (size_t i=0; i<viewports_count; i++)
  {
      //setup viewport context

    const SceneViewport& scene_viewport = viewports[i];

    ViewportContextBindings viewport_bindings(context);

    viewport_bindings.bindings.bind(&renderer_bindings);
    viewport_bindings.bindings.bind(scene_viewport.properties());
    viewport_bindings.bindings.bind(scene_viewport.textures());

      //set camera

    context.set_view_node(scene_viewport.camera());

      //set framebuffer

    const Viewport& viewport = scene_viewport.viewport();

    if (!viewport.width && !viewport.height)
    {
      window_frame_buffer.reset_viewport();
    }
    else
    {
      window_frame_buffer.set_viewport(viewport);
    }

      //render passes

    for (auto& pass_entry : impl->passes)
    {
      impl->render_pass(pass_entry);
    }

      //render frame nodes

    context.root_frame_node().render(context);
  }
}

PropertyMap& SceneRenderer::properties() const
{
  return impl->shared_properties;
}

TextureList& SceneRenderer::textures() const
{
  return impl->shared_textures;
}

MaterialList& SceneRenderer::materials() const
{
  return impl->shared_materials;
}

FrameNodeList& SceneRenderer::frame_nodes() const
{
  return impl->shared_frame_nodes;
}
