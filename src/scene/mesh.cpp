#include <scene/mesh.h>

using namespace engine::scene;

/// Mesh implementation details
struct Mesh::Impl
{
  media::geometry::Mesh mesh;
};

Mesh::Mesh()
  : impl(std::make_unique<Impl>())
{
}

Mesh::Pointer Mesh::create()
{
  return Mesh::Pointer(new Mesh);
}

void Mesh::set_mesh(const media::geometry::Mesh& mesh)
{
  impl->mesh = mesh;
}

const engine::media::geometry::Mesh& Mesh::mesh() const
{
  return impl->mesh;
}

engine::media::geometry::Mesh& Mesh::mesh()
{
  return impl->mesh;
}

void Mesh::visit(ISceneVisitor& visitor)
{
  Node::visit(visitor);

  visitor.visit(*this);
}
