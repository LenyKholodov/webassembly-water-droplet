#include "shared.h"

using namespace engine::scene;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::common;

///
/// Constants
///

const size_t RESERVED_PASSES_COUNT = 16; // number of reserved passes per scene renderer
// const size_t MAX_NESTED_RENDER_DEPTH = 2; //maximum number of nested renderings
const size_t MAX_NESTED_RENDER_DEPTH = 1; // maximum number of nested renderings

///
/// SceneViewport
///

/// Implementation details of scene viewport
struct SceneViewport::Impl
{
  Node::Pointer view_node;   // camera of the scene viewport
  math::mat4f projection_tm; // projection matrix of the scene viewport
  math::mat4f subview_tm;    // subview matrix of the scene viewport
  FrameBuffer frame_buffer;  // framebuffer of the scene viewport
  Viewport viewport;         // viewport of the scene viewport
  math::vec4f clear_color;   // clear color of the scene viewport
  PropertyMap properties;    // viewport properties;
  TextureList textures;      // viewport textures;
  std::shared_ptr<ScenePassOptions> options; // rendering options

  Impl(const FrameBuffer &frame_buffer)
      : projection_tm(1.0f), subview_tm(1.0f), frame_buffer(frame_buffer), clear_color(0.0f, 0.0f, 0.0f, 1.0f)
  {
  }
};

SceneViewport::SceneViewport(const FrameBuffer &frame_buffer)
    : impl(std::make_shared<Impl>(frame_buffer))
{
}

const Viewport &SceneViewport::viewport() const
{
  return impl->viewport;
}

void SceneViewport::set_viewport(const Viewport &viewport)
{
  impl->viewport = viewport;
}

const engine::render::low_level::FrameBuffer &SceneViewport::frame_buffer() const
{
  return impl->frame_buffer;
}

void SceneViewport::set_frame_buffer(const low_level::FrameBuffer &frame_buffer)
{
  impl->frame_buffer = frame_buffer;
}

void SceneViewport::set_clear_color(const math::vec4f &color)
{
  impl->clear_color = color;
}

const math::vec4f &SceneViewport::clear_color() const
{
  return impl->clear_color;
}

Node::Pointer &SceneViewport::view_node() const
{
  return impl->view_node;
}

const math::mat4f &SceneViewport::projection_tm() const
{
  return impl->projection_tm;
}

const math::mat4f &SceneViewport::subview_tm() const
{
  return impl->subview_tm;
}

void SceneViewport::set_view_node(const scene::Node::Pointer &view_node, const math::mat4f &projection_tm, const math::mat4f &subview_tm)
{
  impl->view_node = view_node;
  impl->projection_tm = projection_tm;
  impl->subview_tm = subview_tm;
}

void SceneViewport::set_view_node(const scene::Camera::Pointer &camera)
{
  set_view_node(camera, camera->projection_matrix());
}

PropertyMap &SceneViewport::properties() const
{
  return impl->properties;
}

void SceneViewport::set_properties(const common::PropertyMap &properties)
{
  impl->properties = properties;
}

TextureList &SceneViewport::textures() const
{
  return impl->textures;
}

void SceneViewport::set_textures(const low_level::TextureList &textures)
{
  impl->textures = textures;
}

const std::shared_ptr<ScenePassOptions>& SceneViewport::options() const
{
  return impl->options;
}

void SceneViewport::set_options(const std::shared_ptr<ScenePassOptions> &options)
{
  impl->options = options;
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
    ScenePassPtr pass;               // scene pass
    std::string name;                // self pass name
    int priority;                    // priority of pass rendering
    PassArray dependencies;          // dependent scene passes
    size_t prerendered_enumeration_id; // prerendered subframe ID
    size_t rendered_enumeration_id;    // rendered subframe ID

    PassEntry(const char *name, const ScenePassPtr &pass, int priority)
        : pass(pass), name(name), priority(priority), prerendered_enumeration_id(), rendered_enumeration_id()
    {
    }

    bool operator<(const PassEntry &other) const { return priority < other.priority; }
  };

  struct ScenePassContextImpl : ScenePassContext
  {
    ScenePassContextImpl(ISceneRenderer &owner)
        : ScenePassContext(owner)
    {
    }

    using ScenePassContext::bind;
    using ScenePassContext::unbind;
  };

  /// Rendering stack entry for recursive scene traversing during prerendering
  struct SceneRenderQueueEntry
  {
    SceneViewport viewport;
    size_t nested_depth = 0;
    FrameId subframe_id = 0;
    ScenePassContextImpl passes_context;
    std::shared_ptr<SceneRenderQueueEntry> first_child;
    std::shared_ptr<SceneRenderQueueEntry> last_child;
    std::shared_ptr<SceneRenderQueueEntry> next_child;

    SceneRenderQueueEntry(ISceneRenderer &renderer, const SceneViewport &viewport, size_t nested_depth = 0)
        : viewport(viewport), nested_depth(nested_depth), passes_context(renderer)
    {
    }

    bool has_children() const { return first_child != nullptr; }

    void reset()
    {
      nested_depth = 0;
      first_child.reset();
      last_child.reset();
      next_child.reset();
    }

    void add_child(FrameId subframe_id, ISceneRenderer &renderer, const SceneViewport &viewport)
    {
      auto child = std::make_shared<SceneRenderQueueEntry>(renderer, viewport, nested_depth + 1);

      child->passes_context.set_options(viewport.options());

      child->subframe_id = subframe_id;

      if (last_child)
      {
        last_child->next_child = child;
        last_child = child;
      }
      else
      {
        first_child = child;
        last_child = child;
      }
    }
  };

}

/// Implementation details of scene renderer
struct SceneRenderer::Impl : ISceneRenderer, public std::enable_shared_from_this<SceneRenderer::Impl>
{
  Device render_device;                        // rendering device
  TextureList shared_textures;                 // shared textures
  MaterialList shared_materials;               // shared materials
  FrameNodeList shared_frame_nodes;            // shared frame_nodes
  common::PropertyMap shared_properties;       // shared propertiess
  PassArray passes;                            // scene rendering passes
  size_t current_subframe_id;                  // subframe ID
  size_t current_enumeration_id;               // enumeration ID
  SceneRenderQueueEntry render_queue_root;     // root of the render queue
  SceneRenderQueueEntry *render_queue_current; // current entry of the render queue
  bool is_in_rendering = false;                // is scene renderer in rendering process
  ScenePassOptions default_options;            // default rendering options

  Impl(const Device &device)
      : render_device(device), current_subframe_id(), current_enumeration_id(), render_queue_root(*this, SceneViewport(render_device.window_frame_buffer())), render_queue_current()
  {
    passes.reserve(RESERVED_PASSES_COUNT);
  }

  struct ViewportContextBindings
  {
    BindingContext bindings;
    ScenePassContextImpl &context;

    ViewportContextBindings(ScenePassContextImpl &context) : context(context) { context.bind(&bindings); }
    ~ViewportContextBindings() { context.unbind(&bindings); }
  };

  void prerender_viewport(SceneRenderQueueEntry &entry)
  {
    struct RenderQueueGuard
    {
      SceneRenderer::Impl &impl;
      SceneRenderQueueEntry *prev_queue_entry;

      RenderQueueGuard(SceneRenderer::Impl &impl, SceneRenderQueueEntry *entry)
          : impl(impl), prev_queue_entry(impl.render_queue_current)
      {
        impl.render_queue_current = entry;
      }

      ~RenderQueueGuard()
      {
        impl.render_queue_current = prev_queue_entry;
      }
    };

    RenderQueueGuard guard(*this, &entry);

    // render passes

    BindingContext renderer_bindings(shared_textures, shared_properties);
    ScenePassContextImpl &context = entry.passes_context;
    SceneViewport &scene_viewport = entry.viewport;

    // sync frame info

    context.set_current_frame_id(render_queue_root.passes_context.current_frame_id());

    // setup viewport context

    ViewportContextBindings viewport_bindings(context);

    viewport_bindings.bindings.bind(&renderer_bindings);
    viewport_bindings.bindings.bind(scene_viewport.properties());
    viewport_bindings.bindings.bind(scene_viewport.textures());

    // sync frame info

    context.set_default_frame_buffer(scene_viewport.frame_buffer());
    context.set_clear_color(scene_viewport.clear_color());

    context.set_current_subframe_id(entry.subframe_id);
    context.set_current_frame_id(render_queue_root.passes_context.current_frame_id());

    // set camera

    context.set_view_node(scene_viewport.view_node(), scene_viewport.projection_tm(), scene_viewport.subview_tm());

    // prerender passes

    current_enumeration_id++;

    context.set_current_enumeration_id(current_enumeration_id);

    for (auto &pass_entry : passes)
    {
      prerender_pass(pass_entry, context);
    }

    // recursive prerender children entries

    for (auto child = entry.first_child; child; child = child->next_child)
    {
      prerender_viewport(*child);
    }
  }

  void render_viewport(SceneRenderQueueEntry &entry)
  {
    // recursive render children entries

    for (auto child = entry.first_child; child; child = child->next_child)
    {
      render_viewport(*child);
    }

    // render passes

    BindingContext renderer_bindings(shared_textures, shared_properties);
    ScenePassContextImpl &context = entry.passes_context;
    SceneViewport &scene_viewport = entry.viewport;
    FrameBuffer frame_buffer = scene_viewport.frame_buffer();

    // sync frame info

    context.set_current_frame_id(render_queue_root.passes_context.current_frame_id());
    context.set_current_subframe_id(entry.subframe_id);

    // setup viewport context

    context.set_default_frame_buffer(scene_viewport.frame_buffer());
    context.set_clear_color(scene_viewport.clear_color());

    // setup viewport context

    ViewportContextBindings viewport_bindings(context);

    viewport_bindings.bindings.bind(&renderer_bindings);
    viewport_bindings.bindings.bind(scene_viewport.properties());
    viewport_bindings.bindings.bind(scene_viewport.textures());

    // set camera

    context.set_view_node(scene_viewport.view_node(), scene_viewport.projection_tm(), scene_viewport.subview_tm());

    // set framebuffer

    const Viewport &viewport = scene_viewport.viewport();

    if (!viewport.width && !viewport.height)
    {
      frame_buffer.reset_viewport();
    }
    else
    {
      frame_buffer.set_viewport(viewport);
    }
    // render passes

    current_enumeration_id++;

    context.set_current_enumeration_id(current_enumeration_id);

    for (auto &pass_entry : passes)
    {
      render_pass(pass_entry, context);
    }

    // render frame nodes

    context.root_frame_node().render(context);
  }

  void prerender_pass(PassEntryPtr &pass_entry, ScenePassContext &context)
  {
    if (pass_entry->prerendered_enumeration_id >= current_enumeration_id)
      return;

    // prerender dependencies

    for (auto &dep_pass_entry : pass_entry->dependencies)
    {
      prerender_pass(dep_pass_entry, context);
    }

    // render pass

    pass_entry->pass->prerender(context);

    // update frame info

    pass_entry->prerendered_enumeration_id = current_enumeration_id;
  }

  void render_pass(PassEntryPtr &pass_entry, ScenePassContext &context)
  {
    if (pass_entry->rendered_enumeration_id >= current_enumeration_id)
      return;

    // render dependencies

    for (auto &dep_pass_entry : pass_entry->dependencies)
    {
      render_pass(dep_pass_entry, context);
    }

    // render pass

    pass_entry->pass->render(context);

    // update frame info

    pass_entry->rendered_enumeration_id = current_enumeration_id;
  }

  PropertyMap &properties() override { return shared_properties; }
  TextureList &textures() override { return shared_textures; }
  MaterialList &materials() override { return shared_materials; }
  FrameNodeList &frame_nodes() override { return shared_frame_nodes; }
  Device &device() override { return render_device; }
  SceneRenderer scene_renderer() override { return SceneRenderer(shared_from_this()); }
  low_level::FrameBuffer &default_frame_buffer() override { return render_device.window_frame_buffer(); }
};

SceneRenderer::SceneRenderer(const Window &window, const DeviceOptions &options)
{
  Device device(window, options);

  impl = std::make_shared<Impl>(device);
}

SceneRenderer::SceneRenderer(const std::shared_ptr<Impl> &impl)
    : impl(impl)
{
}

SceneViewport SceneRenderer::create_window_viewport() const
{
  return SceneViewport(impl->render_device.window_frame_buffer());
}

Device &SceneRenderer::device() const
{
  return impl->render_device;
}

const ScenePassOptions& SceneRenderer::default_options() const
{
  return impl->default_options;
}

size_t SceneRenderer::passes_count() const
{
  return impl->passes.size();
}

namespace
{

  struct PassResolver
  {
    SceneRenderer &renderer;
    Device &device;
    int priority;
    std::string root_pass;
    PassArray passes;
    const PassArray &existing_passes;

    PassResolver(SceneRenderer &renderer, const PassArray &existing_passes, Device &device, const char *root_pass, int priority)
        : renderer(renderer), device(device), priority(priority), root_pass(root_pass), existing_passes(existing_passes)
    {
      passes.reserve(RESERVED_PASSES_COUNT);
    }

    struct StackFrame
    {
      PassEntryPtr pass;
      const char *name;
      StackFrame *prev;

      StackFrame(const char *name, const PassEntryPtr &pass, StackFrame *prev)
          : pass(pass), name(name), prev(prev)
      {
      }
    };

    static PassEntryPtr find_pass(const char *name, const PassArray &passes)
    {
      for (const auto &pass_entry : passes)
      {
        if (pass_entry->name == name)
          return pass_entry;
      }

      return nullptr;
    }

    static bool check_loop(const char *name, StackFrame *parent)
    {
      for (StackFrame *it = parent; it; it = it->prev)
      {
        if (!strcmp(it->name, name))
          return true;
      }

      return false;
    }

    static void sort(PassArray &passes)
    {
      std::stable_sort(passes.begin(), passes.end(), [](const auto &ptr1, const auto &ptr2)
                       { return *ptr1 < *ptr2; });
    }

    PassEntryPtr create_pass(const char *name, StackFrame *parent)
    {
      // check loops

      if (check_loop(name, parent))
      {
        std::string stack = name;

        for (StackFrame *it = parent; it; it = it->prev)
        {
          stack = engine::common::format("%s -> %s", it->name, stack.c_str());
        }

        throw Exception::format("Can't create pass '%s' due to pass depenency loop: %s", root_pass.c_str(), stack.c_str());
      }

      // create pass

      ScenePassPtr pass = ScenePassFactory::create_pass(name, renderer, device);
      PassEntryPtr entry = std::make_shared<PassEntry>(name, pass, priority);
      StackFrame frame(name, entry, parent);

      // create dependencies

      std::vector<std::string> deps;

      pass->get_dependencies(deps);

      for (auto &dep : deps)
      {
        add_pass(dep.c_str(), &frame);
      }

      passes.push_back(entry);

      // add pass to parent

      if (parent && parent->pass)
      {
        parent->pass->dependencies.push_back(entry);

        // TODO: optimize sorting

        sort(parent->pass->dependencies);
      }

      return entry;
    }

    PassEntryPtr add_pass(const char *name, StackFrame *parent)
    {
      if (!parent)
      {
        engine_log_debug("Resolving scene pass '%s'", name);
      }
      else
      {
        engine_log_debug("...creating scene pass '%s' for '%s'", name, root_pass.c_str());
      }

      // check if we have already created such pass

      PassEntryPtr pass = find_pass(name, existing_passes);

      if (!pass)
      {
        pass = find_pass(name, passes);
      }

      // create new pass

      if (!pass)
      {
        pass = create_pass(name, parent);
      }

      return pass;
    }
  };

}

void SceneRenderer::add_pass(const char *name, int priority)
{
  engine_check_null(name);

  // create pass

  PassResolver resolver(*this, impl->passes, impl->render_device, name, priority);

  resolver.add_pass(name, nullptr);

  // register pass

  impl->passes.insert(impl->passes.end(), resolver.passes.begin(), resolver.passes.end());

  PassResolver::sort(impl->passes);
}

void SceneRenderer::remove_pass(const char *name)
{
  if (!name)
    return;

  impl->passes.erase(std::remove_if(impl->passes.begin(), impl->passes.end(), [&](const auto &pass_entry)
                                    { return pass_entry->name == name; }),
                     impl->passes.end());
}

void SceneRenderer::render(const SceneViewport &scene_viewport)
{
  // process nested renderings (recursion)

  if (impl->is_in_rendering)
    throw Exception::format("Can't start nested rendering for scene viewport outside of prerendering stage");

  if (impl->render_queue_current)
  {
    if (impl->render_queue_current->nested_depth >= MAX_NESTED_RENDER_DEPTH)
      return;

    impl->render_queue_current->add_child(++impl->current_subframe_id, *impl, scene_viewport);
    return;
  }

  // set root render queue entry

  impl->render_queue_root.subframe_id = ++impl->current_subframe_id;
  impl->render_queue_root.viewport = scene_viewport;

  // update frame info

  impl->render_queue_root.passes_context.set_current_frame_id(impl->render_queue_root.passes_context.current_frame_id() + 1);
  impl->render_queue_root.passes_context.set_options(scene_viewport.options());

  // prerender viewport

  impl->prerender_viewport(impl->render_queue_root);

  // render viewport

  struct RenderingProcessGuard
  {
    Impl &impl;

    RenderingProcessGuard(Impl &impl) : impl(impl) { impl.is_in_rendering = true; }
    ~RenderingProcessGuard() { impl.is_in_rendering = false; }
  };

  RenderingProcessGuard guard(*impl);

  impl->render_viewport(impl->render_queue_root);

  // cleanup

  impl->render_queue_root.reset();
  impl->render_queue_root.passes_context.set_options(std::shared_ptr<ScenePassOptions>());
}

void SceneRenderer::render(size_t viewports_count, const SceneViewport *viewports)
{
  if (viewports_count)
    engine_check_null(viewports);

  for (size_t i = 0; i < viewports_count; i++)
  {
    render(viewports[i]);
  }
}

PropertyMap &SceneRenderer::properties() const
{
  return impl->shared_properties;
}

TextureList &SceneRenderer::textures() const
{
  return impl->shared_textures;
}

MaterialList &SceneRenderer::materials() const
{
  return impl->shared_materials;
}

FrameNodeList &SceneRenderer::frame_nodes() const
{
  return impl->shared_frame_nodes;
}
