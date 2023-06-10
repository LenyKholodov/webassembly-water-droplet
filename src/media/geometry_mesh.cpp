#include <common/string.h>
#include <common/uninitialized_storage.h>

#include <media/geometry.h>

#include <vector>

using namespace engine::media::geometry;
using namespace engine::common;

typedef std::vector<Primitive> PrimitiveArray;

/// Mesh implementation
struct Mesh::Impl
{
  UninitializedStorage<Vertex> vertices_data;
  UninitializedStorage<index_type> indices_data;
  PrimitiveArray primitives;

  uint32_t add_primitive(const char* material, PrimitiveType type, uint32_t first, uint32_t count, uint32_t base_vertex)
  {
    engine_check_null(material);

    if (type < 0 || type >= PrimitiveType_Num)
      throw Exception(format("Can't add unknown primitive type %d", type).c_str());

    Primitive primitive;

    primitive.type = type;
    primitive.first = first;
    primitive.count = count;
    primitive.base_vertex = base_vertex;
    primitive.material = material;

    primitives.push_back(primitive);

    return (uint32_t)primitives.size() - 1;
  }

  uint32_t add_primitive(const char* material, PrimitiveType type, const Vertex* vertices, index_type vertices_count, const index_type* indices, uint32_t indices_count)
  {
    engine_check_null(material);

      //TODO check vertices/indices count limit

    uint32_t current_vertices_count = static_cast<uint32_t>(vertices_data.size());
    uint32_t current_indices_count = static_cast<uint32_t>(indices_data.size());

      //resize buffers to store additional data

    vertices_data.resize(current_vertices_count + vertices_count);

    try
    {
      indices_data.resize(current_indices_count + indices_count);
    }
    catch (...)
    {
      //restore previous vertices size if indices resize failed
      vertices_data.resize(current_vertices_count);
      throw;
    }

      //copy data

    memcpy(vertices_data.data() + current_vertices_count, vertices, vertices_count * sizeof(Vertex));
    memcpy(indices_data.data() + current_indices_count, indices, indices_count * sizeof(index_type));

      //add primitive

    return add_primitive(material, type, current_indices_count / 3, indices_count / 3, current_vertices_count);
  }

  Mesh merge(const Mesh& mesh)
  {
      //TODO check vertices/indices count limit
      //TODO code can be optimized - copy larger mesh first and smaller mesh second

    Mesh return_value;

    uint32_t vertices_count             = static_cast<uint32_t>(vertices_data.size()),
             indices_count              = static_cast<uint32_t>(indices_data.size()),
             second_mesh_vertices_count = mesh.vertices_count(),
             second_mesh_indices_count  = mesh.indices_count();

      //allocate memory

    return_value.vertices_resize(vertices_count + second_mesh_vertices_count);
    return_value.indices_resize(indices_count + second_mesh_indices_count);

      //copy buffers

    memcpy(return_value.vertices_data(), vertices_data.data(), vertices_count * sizeof(Vertex));
    memcpy(return_value.indices_data(), indices_data.data(), indices_count * sizeof(index_type));
    memcpy(return_value.vertices_data() + vertices_count, mesh.vertices_data(), second_mesh_vertices_count * sizeof(Vertex));
    memcpy(return_value.indices_data() + indices_count, mesh.indices_data(), second_mesh_indices_count * sizeof(index_type));

      //copy primitives

    for (size_t i = 0, count = primitives.size(); i < count; i++)
    {
      const Primitive& primitive = primitives[i];

      return_value.add_primitive(primitive.material.c_str(), primitive.type, primitive.first, primitive.count, primitive.base_vertex);
    }

    for (uint32_t i = 0, count = mesh.primitives_count(); i < count; i++)
    {
      const Primitive& primitive = mesh.primitive(i);

      return_value.add_primitive(primitive.material.c_str(), primitive.type, primitive.first + indices_count / 3, primitive.count, primitive.base_vertex + vertices_count);
    }

    return return_value;
  }

  Mesh merge_primitives()
  {
    Mesh return_value;

    uint32_t vertices_count = static_cast<uint32_t>(vertices_data.size()),
             indices_count  = static_cast<uint32_t>(indices_data.size());

      //allocate memory

    return_value.vertices_resize(vertices_count);
    return_value.indices_resize(indices_count);

      //copy data
    Vertex* first_vertex = return_value.vertices_data();
    index_type* first_index = return_value.indices_data();
    Vertex* current_vertex = first_vertex;
    index_type* current_index = first_index;

    PrimitiveArray primitives_to_process = primitives;

    while (!primitives_to_process.empty())
    {
      std::string current_material = primitives_to_process.back().material;
      PrimitiveType current_primitive_type = primitives_to_process.back().type;
      size_t current_index_index = current_index - first_index;
      size_t current_vertex_index = current_vertex - first_vertex;
      size_t copied_triangles_count = 0;
      size_t copied_vertices_count = 0;

      for (int i = (int)primitives_to_process.size() - 1; i >= 0; i--)
      {
        const Primitive& primitive = primitives_to_process[i];

        if (primitive.material != current_material || primitive.type != current_primitive_type)
          continue;

        size_t primitive_vertices_count = 0;
        size_t primitive_indices_count = primitive.count * 3;

        index_type* first_primitive_index = indices_data.data() + primitive.first * 3;
        index_type* source_index = first_primitive_index;

        for (size_t j = 0; j < primitive_indices_count; j++, source_index++)
          primitive_vertices_count = std::max((size_t)*source_index + 1, primitive_vertices_count);

        memcpy(current_vertex, vertices_data.data() + primitive.base_vertex, primitive_vertices_count * sizeof(Vertex));
        memcpy(current_index, first_primitive_index, primitive_indices_count * sizeof(index_type));

        for (size_t j = 0; j < primitive_indices_count; j++, current_index++)
          *current_index += copied_vertices_count;

        copied_vertices_count += primitive_vertices_count;
        current_vertex += primitive_vertices_count;

        copied_triangles_count += primitive.count;

        primitives_to_process.erase(primitives_to_process.begin() + i);
      }

      return_value.add_primitive(current_material.c_str(), current_primitive_type, (uint32_t)current_index_index, (uint32_t)copied_triangles_count, (uint32_t)current_vertex_index);
    }

    return return_value;
  }
};

/// Mesh

/// Constructor
Mesh::Mesh()
  : impl(new Impl)
  {}

/// Vertices data
uint32_t Mesh::vertices_count() const
{
  return static_cast<uint32_t>(impl->vertices_data.size());
}

void Mesh::vertices_resize(uint32_t vertices_count)
{
  impl->vertices_data.resize(vertices_count);
}

const Vertex* Mesh::vertices_data() const
{
  return impl->vertices_data.data();
}

Vertex* Mesh::vertices_data()
{
  return impl->vertices_data.data();
}

void Mesh::vertices_clear()
{
  impl->vertices_data.resize(0);
}

uint32_t Mesh::vertices_capacity() const
{
  return static_cast<uint32_t>(impl->vertices_data.capacity());
}

void Mesh::vertices_reserve(uint32_t vertices_count)
{
  impl->vertices_data.reserve(vertices_count);
}

/// Indices data
uint32_t Mesh::indices_count() const
{
  return static_cast<uint32_t>(impl->indices_data.size());
}

void Mesh::indices_resize(uint32_t indices_count)
{
  impl->indices_data.resize(indices_count);
}

const Mesh::index_type* Mesh::indices_data() const
{
  return impl->indices_data.data();
}

Mesh::index_type* Mesh::indices_data()
{
  return impl->indices_data.data();
}

void Mesh::indices_clear()
{
  impl->indices_data.resize(0);
}

uint32_t Mesh::indices_capacity() const
{
  return static_cast<uint32_t>(impl->indices_data.capacity());
}

void Mesh::indices_reserve(uint32_t indices_count)
{
  impl->indices_data.reserve(indices_count);
}

/// Primitives data
uint32_t Mesh::primitives_count() const
{
  return static_cast<uint32_t>(impl->primitives.size());
}

const Primitive& Mesh::primitive(uint32_t index) const
{
  if (index >= impl->primitives.size())
    throw Exception(format("engine::media::Mesh::primitive index %u out of bounds [0;%z)", index, impl->primitives.size()).c_str());

  return impl->primitives[index];
}

/// Add / remove primitives
uint32_t Mesh::add_primitive(const char* material, PrimitiveType type, uint32_t first, uint32_t count, uint32_t base_vertex)
{
  return impl->add_primitive(material, type, first, count, base_vertex);
}

uint32_t Mesh::add_primitive(const char* material, PrimitiveType type, const Vertex* vertices, index_type vertices_count, const index_type* indices, uint32_t indices_count)
{
  return impl->add_primitive(material, type, vertices, vertices_count, indices, indices_count);
}

void Mesh::remove_primitive(uint32_t primitive_index)
{
  if (primitive_index >= impl->primitives.size())
    return;

  impl->primitives.erase(impl->primitives.begin() + primitive_index);
}

void Mesh::remove_all_primitives()
{
  impl->primitives.clear();
}

Mesh Mesh::merge(const Mesh& mesh) const
{
  return impl->merge(mesh);
}

Mesh Mesh::merge_primitives() const
{
  return impl->merge_primitives();
}

void Mesh::clear()
{
  remove_all_primitives();
  indices_clear();
  vertices_clear();
}
