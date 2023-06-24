#include <scene/mesh.h>

using namespace engine::scene;

/// Mesh implementation details
struct Mesh::Impl
{
  media::geometry::Mesh mesh;
  size_t first_primitive;
  size_t primitives_count;

  Impl()
    : first_primitive(0)
    , primitives_count((size_t)-1)
  {
  }
};

Mesh::Mesh()
  : impl(std::make_unique<Impl>())
{
}

Mesh::Pointer Mesh::create()
{
  return Mesh::Pointer(new Mesh);
}

void Mesh::set_mesh(const media::geometry::Mesh& mesh, size_t first_primitive, size_t primitives_count)
{
  impl->mesh = mesh;
  impl->first_primitive = first_primitive;
  impl->primitives_count = primitives_count;
}

const engine::media::geometry::Mesh& Mesh::mesh() const
{
  return impl->mesh;
}

engine::media::geometry::Mesh& Mesh::mesh()
{
  return impl->mesh;
}

size_t Mesh::first_primitive() const
{
  return impl->first_primitive;
}

size_t Mesh::primitives_count() const
{
  return impl->primitives_count;
}

void Mesh::visit(ISceneVisitor& visitor)
{
  Entity::visit(visitor);

  visitor.visit(*this);
}
