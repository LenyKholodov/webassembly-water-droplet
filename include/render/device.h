#pragma once

#include <media/geometry.h>

#include <common/string.h>
#include <common/property_map.h>

#include <math/vector.h>

#include <memory>
#include <cstdint>

namespace engine {

namespace application {

//forward declarations
class Window;

}

namespace render {
namespace low_level {

using application::Window;
using media::geometry::Vertex;
using media::geometry::PrimitiveType;
using common::PropertyType;
using common::Property;
using common::PropertyMap;

/// Implementation forwards
class DeviceContextImpl;
struct BufferImpl;
struct ShaderImpl;
struct TextureLevelInfo;
struct RenderBufferInfo;
struct ProgramParameter;

typedef std::shared_ptr<DeviceContextImpl> DeviceContextPtr;

/// Clear flags
enum ClearFlags
{
  Clear_None    = 0,
  Clear_Color   = 1, //clear color buffer
  Clear_Depth   = 2, //clear depth buffer
  Clear_Stencil = 4, //clear stencil buffer

  Clear_DepthStencil = Clear_Depth | Clear_Stencil,
  Clear_All          = Clear_DepthStencil | Clear_Color,
};

/// Shader type
enum ShaderType
{
  ShaderType_Vertex, //vertex shader
  ShaderType_Pixel, //pixel shader
};

/// Pixel format
enum PixelFormat
{
  PixelFormat_RGBA8,
  PixelFormat_RGB16F,
  PixelFormat_D24,
};

/// Texture filter
enum TextureFilter
{
  TextureFilter_Point,
  TextureFilter_Linear,
  TextureFilter_LinearMipLinear,
};

///Compare mode
enum CompareMode
{
  CompareMode_AlwaysFail, //always false
  CompareMode_AlwaysPass, //always true
  CompareMode_Equal, //new_value == reference_value
  CompareMode_NotEqual, //new_value != reference_value
  CompareMode_Less, //new_value <  reference_value
  CompareMode_LessEqual, //new_value <= reference_value
  CompareMode_Greater, //new_value >  reference_value
  CompareMode_GreaterEqual, //new_value >= reference_value

  CompareMode_Num
};

///Blend function argument
enum BlendArgument
{
  BlendArgument_Zero, //0
  BlendArgument_One, //1
  BlendArgument_SourceColor, //source color
  BlendArgument_SourceAlpha, //source alpha
  BlendArgument_InverseSourceColor, //1 - source color
  BlendArgument_InverseSourceAlpha, //1 - source alpha
  BlendArgument_DestinationColor, //destination color
  BlendArgument_DestinationAlpha, //destination alpha
  BlendArgument_InverseDestinationColor, //1 - destination color
  BlendArgument_InverseDestinationAlpha, //1 - destination alpha

  BlendArgument_Num
};

/// Viewport
struct Viewport
{
  int x, y, width, height;

  Viewport(int x=0, int y=0, int width=0, int height=0)
    : x(x), y(y), width(width), height(height) {}
};

/// Texture
class Texture
{
  public:
    /// Constructor
    Texture(const DeviceContextPtr& context, size_t width, size_t height, size_t layers, PixelFormat format, size_t mips_count = (size_t)-1);

    /// Texture width
    size_t width() const;

    /// Texture height
    size_t height() const;

    /// Layers count
    size_t layers() const;

    /// Mipmaps count
    size_t mips_count() const;

    /// Pixel format
    PixelFormat format() const;

    /// Min filter
    TextureFilter min_filter() const;

    /// Set min filter
    void set_min_filter(TextureFilter filter);

    /// Mag filter
    TextureFilter mag_filter() const;

    /// Set mag filter
    void set_mag_filter(TextureFilter filter);

    /// Set texture data
    void set_data(size_t layer, size_t x, size_t y, size_t width, size_t height, const void* data);

    /// Get texture data
    void get_data(size_t layer, size_t x, size_t y, size_t width, size_t height, void* data);

    /// Bind texture to context
    void bind() const;

    /// Generate mipmaps
    void generate_mips();

    /// Get texture level info (internal use only)
    void get_level_info(size_t layer, size_t level, TextureLevelInfo& out_info) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Texture list
class TextureList
{
  public:
    /// List of textures
    TextureList();

    /// Textures count
    size_t count() const;

    /// Add texture
    void insert(const char* name, const Texture& texture);

    /// Remove texture
    void remove(const char* name);

    /// Find texture by name
    Texture* find(const char* name) const;

    /// Get texture by name or throw exception
    Texture& get(const char* name) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Material
class Material
{
  public:
    /// Constructor
    Material();

    /// Textures
    const TextureList& textures() const;

    /// Properties
    const PropertyMap& properties() const;

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

/// Render buffer
class RenderBuffer
{
  public:
    /// Constructor
    RenderBuffer(const DeviceContextPtr& context, size_t width, size_t height, PixelFormat format);

    /// Pixel format
    PixelFormat format() const;

    /// Get render buffer info (internal use only)
    void get_info(RenderBufferInfo& out_info) const;

    /// Bind renderbuffer for rendering to a context
    void bind() const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Frame buffer
class FrameBuffer
{
  public:
    /// Empty buffer creation
    FrameBuffer(const DeviceContextPtr& context);

    /// Constructor
    FrameBuffer(const DeviceContextPtr& context, const Window& window);

    /// Set viewport
    void set_viewport(const Viewport& viewport);

    /// Set viewport based of underlying render buffer / texture size
    void reset_viewport();

    /// Get viewport
    const Viewport& viewport() const;

    /// Number of color targets
    size_t color_targets_count() const;

    /// Attach texture
    void attach_color_target(const Texture& texture, size_t layer = 0, size_t mip_level = 0);

    /// Attach render buffer
    void attach_color_target(const RenderBuffer& render_buffer);

    /// Clear all color attachments
    void detach_all_color_targets();

    /// Attach depth buffer
    void attach_depth_buffer(const Texture& texture, size_t layer = 0, size_t mip_level = 0);

    /// Attach render buffer
    void attach_depth_buffer(const RenderBuffer& render_buffer);

    /// Detach depth buffer
    void detach_depth_buffer();

    /// Bind framebuffer for rendering to a context
    void bind() const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Vertex buffer
/// (simplification: no streams and layouts)
class VertexBuffer
{
  public:
    /// Constructor
    VertexBuffer(const DeviceContextPtr& context, size_t vertices_count);

    /// Vertices count
    size_t vertices_count() const;

    /// Load data
    void set_data(size_t offset, size_t count, const Vertex* vertices);

    /// Bind buffer
    void bind() const;

  private:
    std::shared_ptr<BufferImpl> impl;
};

/// Index buffer
class IndexBuffer
{
  public:
    typedef media::geometry::Mesh::index_type index_type;

    /// Constructor
    IndexBuffer(const DeviceContextPtr& context, size_t indices_count);

    /// Indices count
    size_t indices_count() const;

    /// Load data
    void set_data(size_t offset, size_t count, const index_type* indices);

    /// Bind buffer
    void bind() const;

  private:
    std::shared_ptr<BufferImpl> impl;
};

/// Shader
class Shader
{
  public:
    /// Constructor
    Shader(const DeviceContextPtr& context, ShaderType type, const char* name, const char* source_code);

    /// Name
    const char* name() const;

    /// Shader type
    ShaderType type() const;

    /// Implementation details
    ShaderImpl& get_impl() const;

  private:
    std::shared_ptr<ShaderImpl> impl;
};

/// Program
class Program
{
  public:
    /// Constructor
    Program(const DeviceContextPtr& context, const char* name, const Shader& vertex_shader, const Shader& pixel_shader);

    /// Name of the program
    const char* name() const;

    /// Uniform location
    int find_uniform_location(const char* name) const;

    /// Attribute location
    int find_attribute_location(const char* name) const;

    /// Uniform location (exception if not found)
    int get_uniform_location(const char* name) const;

    /// Attribute location (exception if not found)
    int get_attribute_location(const char* name) const;

    /// Number of parameters
    size_t parameters_count() const;

    /// Parameters
    const ProgramParameter* parameters() const;

    /// Bind
    void bind() const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Rendering primitive
struct Primitive
{
  PrimitiveType type; //type of primitive
  size_t base_vertex; //base vertex offset
  size_t first; //first primitive
  size_t count; //number of primitives for rendering
  VertexBuffer vertex_buffer; //vertex buffer
  IndexBuffer index_buffer; //index buffer
  Material material; //material

  Primitive(const Material& material,
            PrimitiveType type,
            const VertexBuffer& vb,
            const IndexBuffer& ib,
            size_t first,
            size_t count,
            size_t base_vertex = 0)
    : type(type)
    , base_vertex(base_vertex)
    , first(first)
    , count(count)
    , vertex_buffer(vb)
    , index_buffer(ib)
    , material(material)
  {
  }
};

/// Triangle list
struct TriangleList : Primitive
{
  TriangleList(const Material& material,
               const VertexBuffer& vb,
               const IndexBuffer& ib,
               size_t first,
               size_t count,
               size_t base_vertex = 0)
    : Primitive(material, media::geometry::PrimitiveType_TriangleList, vb, ib, first, count, base_vertex)
  {
  }
};

/// Mesh
class Mesh
{
  public:
    /// Constructor
    Mesh(const DeviceContextPtr& context, const media::geometry::Mesh& mesh, const MaterialList& materials);

    /// Primitives count
    size_t primitives_count() const;

    /// Items
    const Primitive* primitives() const;

    /// Get primitive
    const Primitive& primitive(size_t index) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Depth and stencil state description
struct DepthStencilState
{
  bool depth_test_enable; //is depth test enabled
  bool depth_write_enable; //is depth write enabled
  CompareMode depth_compare_mode; //depth compare mode

  //TODO add stencil operations

  DepthStencilState(bool depth_test_enable, bool depth_write_enable, CompareMode depth_compare_mode)
    : depth_test_enable(depth_test_enable)
    , depth_write_enable(depth_write_enable)
    , depth_compare_mode(depth_compare_mode)
    {}
};

/// Blending state descritption
struct BlendState
{
  bool blend_enable; //is blending enabled
  BlendArgument blend_source_argument; //blend function source argument
  BlendArgument blend_destination_argument; //blend function destination argument

  BlendState(bool blend_enable, BlendArgument blend_source_argument, BlendArgument blend_destination_argument)
    : blend_enable(blend_enable)
    , blend_source_argument(blend_source_argument)
    , blend_destination_argument(blend_destination_argument)
    {}
};

/// Binding context for properties and textures (does not control life times)
class BindingContext
{
  public:
    /// Constructors
    BindingContext() = default;

    template <class T1>
    BindingContext(const T1&);

    template <class T1, class T2>
    BindingContext(const T1&, const T2&);

    template <class T1, class T2, class T3>
    BindingContext(const T1&, const T2&, const T3&);

    template <class T1, class T2, class T3, class T4>
    BindingContext(const T1&, const T2&, const T3&, const T4&);

    /// Bind parents
    void bind(const BindingContext*);

    /// Unbind parents
    void unbind(const BindingContext*);

    /// Bind texture list
    void bind(const TextureList&);

    /// Bind property map
    void bind(const PropertyMap&);

    /// Bind material
    void bind(const Material&);

    /// Unbind all
    void unbind_all();

    /// Find property
    const Property* find_property(const char* name) const;

    /// Find texture
    const Texture* find_texture(const char* name) const;

  private:
    template <class T, class Finder>
    static const T* find(const BindingContext* context, const char* name, Finder fn);

  private:
    const BindingContext* parent[2] = {nullptr, nullptr};
    const TextureList* textures = nullptr;
    const PropertyMap* properties = nullptr;
};

/// Pass
class Pass
{
  public:
    /// Constructor
    Pass(const DeviceContextPtr& context,
         const FrameBuffer& frame_buffer,
         const Program& program);

    /// Set framebuffer
    void set_frame_buffer(const FrameBuffer& frame_buffer);

    /// Frame buffer
    FrameBuffer& frame_buffer() const;

    /// Set program
    void set_program(const Program& program);

    /// Program
    Program& program() const;

    /// Set clear color
    void set_clear_color(const math::vec4f& color);

    /// Get clear color
    const math::vec4f& clear_color() const;

    /// Clearing flags
    ClearFlags clear_flags() const;

    /// Set clearing flags
    void set_clear_flags(ClearFlags clear_flags);    

    /// Set depth stencil state
    void set_depth_stencil_state(const DepthStencilState& state);

    /// Get depth stencil state
    const DepthStencilState& depth_stencil_state() const;

    /// Set blend state
    void set_blend_state(const BlendState& state);

    /// Get blend state
    const BlendState& blend_state() const;

    /// Pass properties
    PropertyMap& properties() const;

    /// Pass textures
    TextureList& textures() const;

    /// Number of added primitives
    size_t primitives_count() const;

    //// Default primitive properties
    static common::PropertyMap& default_primitive_properties();

    /// Add primitive to a pass
    void add_primitive(
      const Primitive& primitive,
      const math::mat4f& model_tm = math::mat4f(1.0f),
      const common::PropertyMap& properties = default_primitive_properties());

    /// Add mesh to a pass
    void add_mesh(
      const Mesh& mesh,
      const math::mat4f& model_tm = math::mat4f(1.0f),
      const common::PropertyMap& properties = default_primitive_properties());

    /// Remove all primitives from the pass
    /// will be automaticall called after the Pass::render
    void remove_all_primitives();

    /// Number of reserved primitives
    size_t primitives_capacity() const;

    /// Reserve number of primitives
    void reserve_primitives(size_t count);

    /// Render pass
    void render(const BindingContext* = nullptr);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

/// Device options
struct DeviceOptions
{
  bool vsync; //vertical synchronization enabled
  bool debug; //should we check OpenGL errors and output debug messages

  DeviceOptions()
    : vsync(true)
    , debug(true)
  {
  }
};

/// Rendering device
class Device
{
  public:
    /// Constructor
    Device(const Window& window, const DeviceOptions& options);

    /// Window
    Window& window() const;

    /// Window frame buffer
    FrameBuffer& window_frame_buffer() const;

    /// Create frame buffer
    FrameBuffer create_frame_buffer();

    /// Create vertex buffer
    VertexBuffer create_vertex_buffer(size_t count);

    /// Create index buffer
    IndexBuffer create_index_buffer(size_t count);

    /// Create vertex shader
    Shader create_vertex_shader(const char* name, const char* source_code);

    /// Create pixel shader
    Shader create_pixel_shader(const char* name, const char* source_code);

    /// Create program
    Program create_program(const char* name, const Shader& vertex_shader, const Shader& pixel_shader);

    /// Create program from source code
    Program create_program_from_source(const char* name, const char* source_code);

    /// Create program source file
    Program create_program_from_file(const char* file_name);

    /// Create default program
    Program get_default_program() const;

    /// Create pass
    Pass create_pass();

    /// Create pass
    Pass create_pass(const Program& program);

    /// Create mesh
    Mesh create_mesh(const media::geometry::Mesh& mesh, const MaterialList& materials);

    /// Create plane for drawing full-screen sprite
    Primitive create_plane(const Material& material);

    /// Create texture2d
    Texture create_texture2d(size_t width, size_t height, PixelFormat format, size_t mips_count = 100);

    /// Load texture2d
    Texture create_texture2d(const char* image_path, size_t mips_count = 100);

    /// Create render buffer
    RenderBuffer create_render_buffer(size_t width, size_t height, PixelFormat format);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

#include <render/detail/device.inl>

}}}
