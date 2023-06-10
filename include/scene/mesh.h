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

    /// Attach geometry
    void set_mesh(const media::geometry::Mesh& mesh);

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
