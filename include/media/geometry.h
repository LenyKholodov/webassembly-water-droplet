#pragma once

#include <math/vector.h>
#include <common/property_map.h>
#include <common/exception.h>

#include <string>
#include <memory>
#include <cstdint>
#include <typeinfo>

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
  std::string   name;          /// name of the primitive
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

    /// Set primitive name
    void set_primitive_name(uint32_t index, const char* name);

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

    /// Update transaction ID
    size_t update_transaction_id() const;

    /// Increment update transaction ID
    void touch();

    /// Store user data
    template <class T> T& set_user_data(const T& value);

    /// Remove user data
    template <class T> void reset_user_data();

    /// Find user data (nullptr if no attachment)
    template <class T> T* find_user_data() const;

    /// Read user data
    template <class T> T& get_user_data() const;

  private:
    struct UserData;
    template <class T> struct ConcreteUserData;
    typedef std::shared_ptr<UserData> UserDataPtr;

    void set_user_data_core(const std::type_info&, const UserDataPtr&);
    UserDataPtr find_user_data_core(const std::type_info&) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Texture
struct Texture
{
  std::string name; //name of texture
  std::string file_name; //path to texture file

  Texture() {}
  Texture(const char* name, const char* file_name);
};

/// Material
class Material
{
  public:
    /// Constructor
    Material();

    /// Shader tags
    const char* shader_tags() const;

    /// Set shader tags
    void set_shader_tags(const char* tags);

    /// Properties
    const common::PropertyMap& properties() const;

    /// Properties
    common::PropertyMap& properties();

    /// Textures count
    size_t textures_count() const;

    /// Add texture
    size_t add_texture(const char* name, const char* file_name);

    /// Add texture
    size_t add_texture(const Texture&);

    /// Remove texture
    void remove_texture(const char* name);

    /// Remove texture
    void remove_texture(size_t index);

    /// Find texture by name
    Texture* find_texture(const char* name) const;

    /// Get texture by name or throw exception
    Texture& get_texture(size_t index) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Material list
class MaterialList
{
  public:
    /// List of materials
    MaterialList();

    /// Material count
    size_t count() const;

    /// Add material
    void insert(const char* name, const Material& material);

    /// Remove texture
    void remove(const char* name);

    /// Find material by name
    Material* find(const char* name) const;

    /// Get material by name or throw exception
    Material& get(const char* name) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Model
struct Model
{
  MaterialList materials; //materials
  Mesh         mesh; //mesh with primitives
};

/// Mesh factory
class MeshFactory
{
  public:
    /// create simple geometry objects
    static Mesh create_box(const char* material, float width, float height, float depth, const math::vec3f& offset = math::vec3f());
    static Mesh create_sphere(const char* material, float radius, const math::vec3f& offset = math::vec3f());

    /// load OBJ files
    static Model load_obj_model(const char* file_name);
};

#include <media/detail/geometry.inl>

}}}
