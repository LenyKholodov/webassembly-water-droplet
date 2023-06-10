#pragma once

#include <math/vector.h>

#include <string>
#include <memory>
#include <cstdint>

namespace engine {
namespace media {
namespace geometry {

/// Renderable vertex data
struct Vertex
{
  math::vec3f position;
  math::vec3f normal;
  math::vec4f color;
  math::vec2f tex_coord;
};

/// Renderable primitive type
enum PrimitiveType
{
  /// Only triangle list primitive type supported for this demo
  PrimitiveType_TriangleList,  /// List of triangles

  PrimitiveType_Num
};

/// Renderable primitive
struct Primitive
{
  PrimitiveType type;          /// primitive type
  uint32_t      first;         /// first vertex index
  uint32_t      count;         /// primitives count
  uint32_t      base_vertex;   /// base vertex index
  std::string   material;      /// material name
};

/// Mesh
class Mesh
{
  public:
    typedef uint16_t index_type;

    /// Constructor
    Mesh();

    /// Vertices count
    uint32_t vertices_count() const;

    /// Change vertices count
    void vertices_resize(uint32_t vertices_count);

    /// Get vertices data
    const Vertex* vertices_data() const;
    Vertex* vertices_data();

    /// Clear vertices data
    void vertices_clear();

    /// Vertices buffer capacity
    uint32_t vertices_capacity() const;

    /// Change vertices buffer capacity
    void vertices_reserve(uint32_t vertices_count);

    /// Indices count
    uint32_t indices_count() const;

    /// Change indices count
    void indices_resize(uint32_t indices_count);

    /// Get indices data
    const index_type* indices_data() const;
    index_type* indices_data();

    /// Clear indices data
    void indices_clear();

    /// Indices buffer capacity
    uint32_t indices_capacity() const;

    /// Change indices buffer capacity
    void indices_reserve(uint32_t indices_count);

    /// Primitives count
    uint32_t primitives_count() const;

    /// Get primitive
    const Primitive& primitive(uint32_t index) const;

    /// Add primitives
    uint32_t add_primitive(const char* material_name, PrimitiveType type, uint32_t first, uint32_t count, uint32_t base_vertex);
    uint32_t add_primitive(const char* material_name, PrimitiveType type, const Vertex* vertices, index_type vertices_count, const index_type* indices, uint32_t indices_count);

    /// Remove primitive
    void remove_primitive(uint32_t primitive_index);

    /// Remove all primitives
    void remove_all_primitives();

    /// Return mesh containing combined data from this mesh and other mesh
    Mesh merge(const Mesh& mesh) const;

    /// Return optimized mesh containing only one primitive for each material
    Mesh merge_primitives() const;

    /// Clear all data
    void clear();

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Mesh factory
class MeshFactory
{
  public:
    /// create simple geometry objects
    static Mesh create_box(const char* material, float width, float height, float depth, const math::vec3f& offset = math::vec3f());
    static Mesh create_sphere(const char* material, float radius, const math::vec3f& offset = math::vec3f());
};

}}}
