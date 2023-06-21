#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::common;

/// Implementation details of scene pass context
struct ScenePassContext::Impl
{
  ISceneRenderer& renderer; //back reference to the owner
  FrameId current_frame_id; //current frame ID
  FrameId current_subframe_id; //current frame ID
  size_t current_enumeration_id; //current enumeration ID
  BindingContext bindings; //context bindings
  Node::Pointer view_node; //view node
  Node::Pointer root_node; //scene root node
  common::PropertyMap properties; //pass self properties
  math::mat4f view_tm; //view matrix
  math::mat4f projection_tm; //projection matrix
  math::mat4f view_projection_tm; //projection * view matrix
  FrameNode root_frame_node; //root frame node
  FrameBuffer default_frame_buffer; //default frame buffer
  math::vec4f clear_color; //clear color
  std::shared_ptr<ScenePassOptions> options; //pass options

  Impl(ISceneRenderer& renderer)
    : renderer(renderer)
    , current_frame_id()
    , current_subframe_id()
    , current_enumeration_id()
    , view_tm(1.0f)
    , projection_tm(1.0f)
    , view_projection_tm(1.0f)
    , default_frame_buffer(renderer.default_frame_buffer())
    , clear_color(0.0f, 0.0f, 0.0f, 1.0f)
  {
  }
};

ScenePassContext::ScenePassContext(ISceneRenderer& renderer)
  : impl(std::make_shared<Impl>(renderer))
{
  impl->bindings.bind(impl->properties);
}

Device& ScenePassContext::device() const
{
  return impl->renderer.device();
}

FrameNode& ScenePassContext::root_frame_node() const
{
  return impl->root_frame_node;
}

FrameId ScenePassContext::current_frame_id() const
{
  return impl->current_frame_id;
}

void ScenePassContext::set_current_frame_id(FrameId id)
{
  impl->current_frame_id = id;
}

FrameId ScenePassContext::current_subframe_id() const
{
  return impl->current_subframe_id;
}

void ScenePassContext::set_current_subframe_id(FrameId id)
{
  impl->current_subframe_id = id;
}

size_t ScenePassContext::current_enumeration_id() const
{
  return impl->current_enumeration_id;
}

void ScenePassContext::set_current_enumeration_id(size_t id)
{
  impl->current_enumeration_id = id;
}

void ScenePassContext::bind(const low_level::BindingContext* parent)
{
  impl->bindings.bind(parent);
}

void ScenePassContext::unbind(const low_level::BindingContext* parent)
{
  impl->bindings.unbind(parent);
}

BindingContext& ScenePassContext::bindings() const
{
  return impl->bindings;
}

FrameNodeList& ScenePassContext::frame_nodes() const
{
  return impl->renderer.frame_nodes();
}

PropertyMap& ScenePassContext::properties() const
{
  return impl->renderer.properties();
}

TextureList& ScenePassContext::textures() const
{
  return impl->renderer.textures();
}

MaterialList& ScenePassContext::materials() const
{
  return impl->renderer.materials();
}

Node::Pointer ScenePassContext::root_node() const
{
  return impl->root_node;
}

Node::Pointer ScenePassContext::view_node() const
{
  return impl->view_node;
}

void ScenePassContext::set_view_node(const Node::Pointer& view, const math::mat4f& projection_tm, const math::mat4f& subview_tm)
{
  impl->view_node = view;

  if (!view)
  {
    impl->root_node = nullptr;
    impl->view_tm = subview_tm;
    impl->projection_tm = projection_tm;
    impl->view_projection_tm = projection_tm * inverse(subview_tm);

    return;
  }

  impl->root_node = view->root();
  impl->view_tm = inverse(view->world_tm() * subview_tm);
  impl->projection_tm = projection_tm;
  impl->view_projection_tm = projection_tm * impl->view_tm;

  math::vec3f world_view_position = view->world_tm() * subview_tm * math::vec3f(0.0f);

  impl->properties.set("viewMatrix", impl->view_tm);
  impl->properties.set("worldViewPosition", world_view_position);
  impl->properties.set("projectionMatrix", impl->projection_tm);
  //impl->properties.set("viewProjectionMatrix", impl->view_projection_tm);
}

void ScenePassContext::set_view_node(const Camera::Pointer& view)
{
  if (view)
  {
    set_view_node(view, view->projection_matrix());
  }
  else
  {
    set_view_node(nullptr, math::mat4f(1.0f));
  }
}

const math::mat4f& ScenePassContext::view_tm() const
{
  return impl->view_tm;
}

const math::mat4f& ScenePassContext::projection_tm() const
{
  return impl->projection_tm;
}

const math::mat4f& ScenePassContext::view_projection_tm() const
{
  return impl->view_projection_tm;
}

const engine::render::low_level::FrameBuffer& ScenePassContext::default_frame_buffer() const
{
  return impl->default_frame_buffer;
}

void ScenePassContext::set_default_frame_buffer(const engine::render::low_level::FrameBuffer& frame_buffer)
{
  impl->default_frame_buffer = frame_buffer;
}

const math::vec4f& ScenePassContext::clear_color() const
{
  return impl->clear_color;
}

void ScenePassContext::set_clear_color(const math::vec4f& color)
{
  impl->clear_color = color;
}

SceneRenderer ScenePassContext::renderer() const
{
  return impl->renderer.scene_renderer();
}

const ScenePassOptions& ScenePassContext::options() const
{
  return impl->options ? *impl->options : renderer().default_options();
}

void ScenePassContext::set_options(const std::shared_ptr<ScenePassOptions>& options)
{
  impl->options = options;
}
