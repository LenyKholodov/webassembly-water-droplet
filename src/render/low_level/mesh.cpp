#include "shared.h"

#include <vector>

using namespace engine::render::low_level;

typedef std::vector<Primitive> PrimitiveArray;

/// Implementation details of mesh
struct Mesh::Impl
{
  DeviceContextPtr context; //device context
  VertexBuffer vertex_buffer; //vertex buffer
  IndexBuffer index_buffer; //index buffer
  PrimitiveArray primitives; //primitives
  MaterialList materials; //list of materials
  size_t update_transaction_id; //ID of the last mesh transaction

  Impl(const DeviceContextPtr& context, const media::geometry::Mesh& mesh, const MaterialList& materials)
    : context(context)
    , vertex_buffer(context, mesh.vertices_count())
    , index_buffer(context, mesh.indices_count())
    , materials(materials)
    , update_transaction_id(mesh.update_transaction_id())
  {
    primitives.reserve(mesh.primitives_count());

    vertex_buffer.set_data(0, mesh.vertices_count(), mesh.vertices_data());
    index_buffer.set_data(0, mesh.indices_count(), mesh.indices_data());

    for (uint32_t i = 0, count = mesh.primitives_count(); i < count; i++)
    {
      const media::geometry::Primitive& src_primitive = mesh.primitive(i);
      Material material = materials.get(src_primitive.material.c_str());

      primitives.emplace_back(Primitive(material, src_primitive.type, vertex_buffer, index_buffer, src_primitive.first, src_primitive.count, src_primitive.base_vertex));
    }
  }
};

Mesh::Mesh(const DeviceContextPtr& context, const media::geometry::Mesh& mesh, const MaterialList& materials)
  : impl(std::make_shared<Impl>(context, mesh, materials))
{
}

size_t Mesh::primitives_count() const
{
  return impl->primitives.size();
}

const Primitive* Mesh::primitives() const
{
  if (impl->primitives.empty())
    return nullptr;

  return &impl->primitives[0];
}

const Primitive& Mesh::primitive(size_t index) const
{
  if (index >= impl->primitives.size())
    throw common::Exception(common::format("engine::render::Mesh::primitive index %u out of bounds [0;%z)", index, impl->primitives.size()).c_str());

  return impl->primitives[index];
}

void Mesh::update_geometry(const media::geometry::Mesh& src_mesh)
{
  if (src_mesh.update_transaction_id() == impl->update_transaction_id)
    return;

  if (src_mesh.vertices_count() > impl->vertex_buffer.vertices_count())
  {
    impl->vertex_buffer.resize(src_mesh.vertices_count());
  }

  if (src_mesh.indices_count() > impl->index_buffer.indices_count())
  {
    impl->index_buffer.resize(src_mesh.indices_count());
  }

  impl->vertex_buffer.set_data(0, src_mesh.vertices_count(), src_mesh.vertices_data());
  impl->index_buffer.set_data(0, src_mesh.indices_count(), src_mesh.indices_data());

  impl->primitives.clear();
  impl->primitives.reserve(src_mesh.primitives_count());

  for (uint32_t i=0, count=src_mesh.primitives_count(); i<count; i++)
  {
    const media::geometry::Primitive& src_primitive = src_mesh.primitive(i);
    Material material = impl->materials.get(src_primitive.material.c_str());

    impl->primitives.emplace_back(Primitive(material, src_primitive.type, impl->vertex_buffer, impl->index_buffer, src_primitive.first, src_primitive.count, src_primitive.base_vertex));
  }

  impl->update_transaction_id = src_mesh.update_transaction_id();
}
