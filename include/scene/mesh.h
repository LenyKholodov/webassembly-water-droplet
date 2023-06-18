#pragma once

#include <media/geometry.h>
#include <scene/node.h>

namespace engine {
namespace scene {

/// Mesh
class Mesh: public Node
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
