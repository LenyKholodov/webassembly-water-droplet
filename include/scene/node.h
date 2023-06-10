#pragma once

#include <scene/visitor.h>

#include <common/exception.h>

#include <math/matrix.h>
#include <math/quat.h>

#include <memory>
#include <typeinfo>

namespace engine {
namespace scene {

/// Scene node
class Node : public std::enable_shared_from_this<Node>
{
  public: 
    typedef std::shared_ptr<Node> Pointer;

    /// Create node
    static Pointer create();

    /// No copy
    Node(const Node&) = delete;
    Node& operator =(const Node&) = delete;

    /// Root node
    Pointer root() const;

    /// Node parent
    Pointer parent() const;

    /// First node child
    Pointer first_child() const;

    /// Last node child
    Pointer last_child() const;

    /// Prev node (within parent's children chain)
    Pointer prev_child() const;

    /// Next node (within parent's children chain)
    Pointer next_child() const;

    /// Bind to parent
    void bind_to_parent(Node& parent);

    /// Unbind from parent
    void unbind();

    /// Unbind all children
    void unbind_all_children();

    /// Node local position
    const math::vec3f& position() const;

    /// Set node local position    
    void set_position(const math::vec3f&);
    
    /// Node local orientation
    const math::quatf& orientation() const;

    /// Set node local orientation
    void set_orientation(const math::quatf&);

    /// Node scale
    const math::vec3f& scale() const;

    /// Set node scale
    void set_scale(const math::vec3f&);

    /// Look to
    void look_to(const math::vec3f& target_point, const math::vec3f& up);

    /// Look to in world space
    void world_look_to(const math::vec3f& target_point, const math::vec3f& up);

    /// Local space node transformations
    const math::mat4f& local_tm() const;

    /// World space node transformations
    const math::mat4f& world_tm() const;

    /// Visit scene
    void traverse(ISceneVisitor&) const;

    /// Store user data
    template <class T> T& set_user_data(const T& value);

    /// Remove user data
    template <class T> void reset_user_data();

    /// Find user data (nullptr if no attachment)
    template <class T> T* find_user_data() const;

    /// Read user data
    template <class T> T& get_user_data() const;
    
  protected:
    /// Constructor
    Node();

    /// Destructor
    virtual ~Node();

    /// Visit node
    virtual void visit(ISceneVisitor&);

  private:
    struct UserData;
    template <class T> struct ConcreteUserData;
    typedef std::shared_ptr<UserData> UserDataPtr;

    void set_user_data_core(const std::type_info&, const UserDataPtr&);
    UserDataPtr find_user_data_core(const std::type_info&) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#include <scene/detail/node.inl>

}}
