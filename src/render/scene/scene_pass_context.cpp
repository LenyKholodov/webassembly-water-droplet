#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::common;

/// Implementation details of scene pass context
struct ScenePassContext::Impl
{
  ISceneRenderer& renderer; //back reference to the owner
  FrameId current_frame_id; //current frame ID
  BindingContext bindings; //context bindings
  Node::Pointer view_node; //view node
  Node::Pointer root_node; //scene root node
  common::PropertyMap properties; //pass self properties
  math::mat4f view_tm; //view matrix
  math::mat4f projection_tm; //projection matrix
  math::mat4f view_projection_tm; //projection * view matrix
  FrameNode root_frame_node; //root frame node

  Impl(ISceneRenderer& renderer)
    : renderer(renderer)
    , current_frame_id()
    , view_tm(1.0f)
    , projection_tm(1.0f)
    , view_projection_tm(1.0f)
  {
  }
};

ScenePassContext::ScenePassContext(ISceneRenderer& renderer)
  : impl(std::make_shared<Impl>(renderer))
{
  impl->bindings.bind(impl->properties);
}

FrameId ScenePassContext::current_frame_id() const
{
  return impl->current_frame_id;
}

Device& ScenePassContext::device() const
{
  return impl->renderer.device();
}

FrameNode& ScenePassContext::root_frame_node() const
{
  return impl->root_frame_node;
}

void ScenePassContext::set_current_frame_id(FrameId id)
{
  impl->current_frame_id = id;
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

void ScenePassContext::set_view_node(const Node::Pointer& view, const math::mat4f& projection_tm)
{
  impl->view_node = view;

  if (!view)
  {
    impl->root_node = nullptr;
    impl->view_tm = 1.0f;
    impl->projection_tm = 1.0f;
    impl->view_projection_tm = 1.0f;

    return;
  }

  impl->root_node = view->root();
  impl->view_tm = inverse(view->world_tm());
  impl->projection_tm = projection_tm;
  impl->view_projection_tm = projection_tm * impl->view_tm;

  math::vec3f world_view_position = view->world_tm() * math::vec3f(0.0f);

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
