#include <scene/node.h>
#include <common/exception.h>
#include <math/utility.h>

#include <unordered_map>

using namespace engine::common;
using namespace engine::scene;

/// Scene node
struct Node::Impl
{
  typedef std::unordered_map<const std::type_info*, UserDataPtr> UserDataMap;

  Node* this_node; //pointer to this node
  std::weak_ptr<Node> parent; //parent node
  Node::Pointer first_child; //first child
  Node::Pointer last_child; //last child
  Node::Pointer prev_child; //previous child on same hierarchy level
  Node::Pointer next_child; //next child on same hierarchy level
  math::vec3f position; // node local position
  math::quatf orientation; //node local orientation
  math::vec3f scale; //node local scale
  math::mat4f local_tm; //local transformation matrix
  math::mat4f world_tm; //world transformation matrix
  bool is_local_tm_dirty; //is local transformation matrix needs update
  bool is_world_tm_dirty; //is world transformation matrix needs update
  UserDataMap user_data_map;

  /// Constructor
  Impl (Node* this_node)
    : this_node(this_node)
    , scale(1.f)
    , is_local_tm_dirty(true)
    , is_world_tm_dirty(true)
  {
  }

  /// Mark transformations as dirty
  void invalidate_matrix()
  {    
    is_local_tm_dirty = true;

    if (!is_world_tm_dirty)
    {
      is_world_tm_dirty = true;

      for (Node::Pointer it=first_child; it; it=it->impl->next_child)
        if (!it->impl->is_world_tm_dirty)
          it->impl->invalidate_matrix();
    }
  }

  void bind_to_parent(Node* new_parent)
  {
    Node::Pointer current_parent = parent.lock();

    if (current_parent.get() == new_parent)
      return;

      //check we do not try to bind to our child
      
    for (Node* node = new_parent; node; node = node->parent().get())
      if (node == this_node)
        throw Exception::format("Attempt to bind object to one of it's child");

      //capture this node, so it will not be deleted because of unbinding from current parent

    Pointer node_lock(this_node->shared_from_this());

      //unbind from current parent

    if (current_parent)
    {
      if (prev_child) prev_child->impl->next_child = next_child;
      else            current_parent->impl->first_child = next_child;

      if (next_child) next_child->impl->prev_child = prev_child;
      else            current_parent->impl->last_child = prev_child;
    }

      //bing to new parent

    if (new_parent)
    {
      parent = new_parent->shared_from_this();

        //регистрируем узел в списке потомков родителя

      Impl* parent_impl = new_parent->impl.get();

      prev_child = parent_impl->last_child;
      next_child = 0;

      parent_impl->last_child = this_node->shared_from_this();

      if (prev_child) prev_child->impl->next_child = this_node->shared_from_this();
      else            parent_impl->first_child     = this_node->shared_from_this();
    }
    else
    {
      parent.reset();
      prev_child = next_child = 0;
    }

    is_world_tm_dirty = true;
  }
};

Node::Node()
  : impl(std::make_unique<Impl>(this))
{
}

Node::~Node()
{
}

Node::Pointer Node::create()
{
  return Node::Pointer(new Node, [](Node* node) { delete node; });
}

Node::Pointer Node::parent() const
{
  return impl->parent.lock();
}

Node::Pointer Node::root() const
{
  Node::Pointer root = const_cast<Node&>(*this).shared_from_this();

  for (;;)
  {
    Node::Pointer parent = root->parent();

    if (!parent)
      return root;

    root = parent;
  }
}

Node::Pointer Node::first_child() const
{
  return impl->first_child;
}

Node::Pointer Node::last_child() const
{
  return impl->last_child;
}

Node::Pointer Node::prev_child() const
{
  return impl->prev_child;
}

Node::Pointer Node::next_child() const
{
  return impl->next_child;
}

void Node::bind_to_parent(Node& parent)
{
  impl->bind_to_parent(&parent);
}

void Node::unbind()
{
  impl->bind_to_parent(nullptr);
}

void Node::unbind_all_children()
{
  while (impl->last_child)
    impl->last_child->unbind ();
}

void Node::set_position(const math::vec3f& position)
{
  impl->position = position;

  impl->invalidate_matrix();
}

const math::vec3f& Node::position() const
{
  return impl->position;
}

void Node::set_orientation(const math::quatf& orientation)
{
  impl->orientation = orientation;

  impl->invalidate_matrix();
}

const math::quatf& Node::orientation() const
{
  return impl->orientation;
}

void Node::set_scale(const math::vec3f& scale)
{
  impl->scale = scale;

  impl->invalidate_matrix();
}

const math::vec3f& Node::scale() const
{
  return impl->scale;
}

void Node::look_to(const math::vec3f& target_point, const math::vec3f& up)
{
  math::vec3f x, y, z;

  z = normalize(target_point);
  y = normalize(up);

  static const float EPS = 0.001f;
  
  if (qlen(y) < EPS || qlen(z) < EPS || equal(y, z, EPS))
    return;

  math::mat3f view;

  x = cross(y, z);
  y = cross(z, x);

  view[0] = x;
  view[1] = y;
  view[2] = z;
  view    = transpose(view);  
  
  math::quatf rotation = -normalize(to_quat(view));  

  set_orientation(rotation * impl->orientation);
}

void Node::world_look_to(const math::vec3f& target_point, const math::vec3f& up)
{
  math::mat4f inv_world_tm = inverse(world_tm());
  math::vec3f local_target_point = inv_world_tm * target_point;
  math::vec3f local_up = math::vec3f(inv_world_tm * math::vec4f(up, 0));

  look_to(local_target_point, local_up);
}

const math::mat4f& Node::local_tm() const
{
  if (impl->is_local_tm_dirty)
  {
    affine_compose(impl->position, impl->orientation, impl->scale, impl->local_tm);

    impl->is_local_tm_dirty = false;
  }

  return impl->local_tm;
}

const math::mat4f& Node::world_tm() const
{
  Pointer current_parent = parent();
    
  if (!current_parent)
    return local_tm();

  if (impl->is_world_tm_dirty)
  {
    impl->world_tm = current_parent->world_tm() * local_tm();
    impl->is_world_tm_dirty = false;
  }

  return impl->world_tm;
}

void Node::traverse(ISceneVisitor& visitor) const
{ 
  const_cast<Node&>(*this).visit(visitor);

  for (Node::Pointer it=first_child(); it; it=it->next_child())
    it->traverse(visitor);
}

void Node::visit(ISceneVisitor& visitor)
{
  visitor.visit(*this);
}

void Node::set_user_data_core(const std::type_info& type, const UserDataPtr& user_data)
{
  if (!user_data)
  {
    impl->user_data_map.erase(&type);
    return;
  }

  impl->user_data_map[&type] = user_data;
}

Node::UserDataPtr Node::find_user_data_core(const std::type_info& type) const
{
  auto it = impl->user_data_map.find(&type);

  if (it == impl->user_data_map.end())
    return nullptr;

  return it->second;
}
