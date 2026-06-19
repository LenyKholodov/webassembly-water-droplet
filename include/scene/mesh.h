#pragma once

#include <media/geometry.h>
#include <scene/node.h>

namespace engine {
namespace scene {

/// Entity
class Entity: public Node
{
  public:
    typedef std::shared_ptr<Entity> Pointer;

    /// Environment maps
    bool is_environment_map_required() const;

    /// Set environment maps requirement
    void set_environment_map_required(bool state);

    /// Environment map rendering local point
    const math::vec3f& environment_map_local_point() const;

    /// Set environment map rendering local point
    void set_environment_map_local_point(const math::vec3f& point);

    /// Planar reflection/refraction (a flat water/mirror surface)
    bool is_planar_reflection_required() const;

    /// Set planar reflection/refraction requirement
    void set_planar_reflection_required(bool state);

    /// Exclude this entity from planar reflection renders (e.g. below-water geometry that would occlude the mirror camera)
    bool is_reflection_excluded() const;

    /// Set reflection exclusion
    void set_reflection_excluded(bool state);

  protected:
    /// Constructor
    Entity();

    /// Destructor
    ~Entity();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Mesh
class Mesh: public Entity
{
  public:
    typedef std::shared_ptr <Mesh> Pointer;

    /// Create
    static Pointer create();
    
    /// Attached geometry
    const media::geometry::Mesh& mesh() const;
    media::geometry::Mesh& mesh();

    /// Primitives range
    size_t first_primitive() const;
    size_t primitives_count() const;

    /// Attach geometry
    void set_mesh(const media::geometry::Mesh& mesh, size_t first_primitive=0, size_t primitives_count=(size_t)-1);

  protected:
    /// Constructor
    Mesh();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}}
