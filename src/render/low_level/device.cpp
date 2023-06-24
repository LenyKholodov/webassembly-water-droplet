#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

///
/// Utilities
///

/// Vertex array object
/// TODO: should be created dynamically for tuple (program, vertex buffer)
struct VertexArrayObject
{
  GLuint id;

  VertexArrayObject()
    : id()
  {
    glGenVertexArrays(1, &id);
    glBindVertexArray(id);
  }

  ~VertexArrayObject()
  {
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &id);
  }
};

/// Implementation details of device
struct Device::Impl
{
  DeviceContextPtr context; //rendering context
  Window window; //application window
  FrameBuffer window_frame_buffer; //window frame buffer
  std::unique_ptr<Program> default_program; //default program
  std::unique_ptr<VertexArrayObject> vertex_array_object; //dummy implementation to make OpenGL 4.1 happy; should be integrated with input layouts

  Impl(const Window& window, const DeviceOptions& options)
    : context(std::make_shared<DeviceContextImpl>(window, options))
    , window(window)
    , window_frame_buffer(context, window)
  {
    context->make_current();

      //common context setup

    vertex_array_object = std::make_unique<VertexArrayObject>();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
  }

  ~Impl()
  {
    try
    {
      context->make_current();

      vertex_array_object.reset();
    }
    catch (...)
    {
      //ignore exceptions in destructors
    }
  }
};

Device::Device(const Window& window, const DeviceOptions& options)
  : impl(new Impl(window, options))
{
    //initialize default program

#ifndef __EMSCRIPTEN__
  static const char* DEFAULT_PROGRAM_SOURCE_CODE = 
    "#shader vertex\n"
    "#version 410 core\n"
    "in vec4 vColor;\n"
    "in vec3 vPosition;\n"
    "out vec4 color;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = vec4(vPosition, 1.0);\n"
    "  color = vColor;\n"
    "}\n"
    "#shader pixel\n"
    "#version 410 core\n"
    "in vec4 color;\n"
    "out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "  outColor = color;\n"
    "}\n";
#else
  static const char* DEFAULT_PROGRAM_SOURCE_CODE = 
    "#shader vertex\n"
    "precision mediump float;\n"
    "uniform vec4 vColor;\n"
    "uniform vec3 vPosition;\n"
    "varying vec4 color;\n"
    "void main()\n"
    "{\n"
    "  gl_Position = vec4(vPosition, 1.0);\n"
    "  color = vColor;\n"
    "}\n"
    "#shader pixel\n"
    "precision mediump float;\n"
    "varying vec4 color;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = color;\n"
    "}\n";
#endif

  impl->default_program = std::make_unique<Program>(create_program_from_source("default", DEFAULT_PROGRAM_SOURCE_CODE));
}

Window& Device::window() const
{
  return impl->window;
}

FrameBuffer& Device::window_frame_buffer() const
{
  return impl->window_frame_buffer;
}

FrameBuffer Device::create_frame_buffer()
{
  return FrameBuffer(impl->context);
}

Texture Device::create_texture2d(size_t width, size_t height, PixelFormat format, size_t mips_count)
{
  return Texture(impl->context, width, height, 1, format, mips_count);
}

Texture Device::create_texture_cubemap(size_t width, size_t height, PixelFormat format, size_t mips_count)
{
  return Texture(impl->context, width, height, 6, format, mips_count);
}

Texture Device::create_texture2d(const char* image_path, size_t mips_count)
{
  media::image::Image image(image_path);
  Texture texture = create_texture2d(image.width(), image.height(), PixelFormat_RGBA8, mips_count);

  texture.set_data(0, 0, 0, image.width(), image.height(), image.bitmap());
  //texture.generate_mips();

  return texture;
}

Texture Device::create_texture_cubemap(const char* image_path, size_t mips_count)
{
  engine_check_null(image_path);

  std::vector<media::image::Image> images;

  const char* end = image_path + strlen(image_path);
  const char* s = end;

  while (s != image_path && *s != '.') --s;

  if (s == image_path)
    s = end;

  static const char* FACES[] = {
    "_posx",
    "_negx",
    "_posy",
    "_negy",
    "_posz",
    "_negz"
  };

  for (size_t i=0; i<6; ++i)
  {
    std::string full_path(image_path, s);

    full_path += FACES[i];
    full_path += s;

    images.emplace_back(media::image::Image(full_path.c_str()));

    if (i > 0)
    {
      engine_check(images[i].width() == images[0].width());
      engine_check(images[i].height() == images[0].height());
    }
  }

  Texture texture = create_texture_cubemap(images[0].width(), images[0].height(), PixelFormat_RGBA8, mips_count);

  for (size_t i=0; i<6; ++i)
  {
    media::image::Image& image = images[i];
    texture.set_data(i, 0, 0, image.width(), image.height(), image.bitmap());    
  }

  //texture.generate_mips();

  return texture;
}

VertexBuffer Device::create_vertex_buffer(size_t count)
{
  return VertexBuffer(impl->context, count);
}

IndexBuffer Device::create_index_buffer(size_t count)
{
  return IndexBuffer(impl->context, count);
}

Shader Device::create_vertex_shader(const char* name, const char* source_code, int lineno_offset)
{
  return Shader(impl->context, ShaderType_Vertex, name, source_code, lineno_offset);
}

Shader Device::create_pixel_shader(const char* name, const char* source_code, int lineno_offset)
{
  return Shader(impl->context, ShaderType_Pixel, name, source_code, lineno_offset);
}

Program Device::create_program(const char* name, const Shader& vertex_shader, const Shader& pixel_shader)
{
  return Program(impl->context, name, vertex_shader, pixel_shader);
}

namespace
{

const char* strstr_with_line_numbers(const char* s, const char* tag, int& lineno)
{
  const char* p = s;
  const char* q = tag;

  for (; *p != '\0'; p++)
  {
    if (*p == '\n')
      lineno++;

    if (*p == *q)
    {
      q++;

      if (*q == '\0')
        return p - strlen(tag) + 1;
    }
    else
      q = tag;
  }

  return nullptr;
}

}

Program Device::create_program_from_source(const char* name, const char* source_code)
{
  engine_check_null(name);
  engine_check_null(source_code);

  struct Source
  {
    std::string source_code;
    int lineno;
  };

  typedef std::unordered_map<std::string, Source> SourceMap;

  SourceMap sources;

    //very basic version of one source combined shader parser

  int lineno = 1;

  auto parser = [&](const char* start_pos) -> const char*
  {
    if (!start_pos)
      return nullptr;

    static const char* SHADER_PRAGMA_TAG = "#shader";

    const char* next_pos = strstr_with_line_numbers(start_pos, SHADER_PRAGMA_TAG, lineno);
    const char* tag_line_start = next_pos;

      //find the next line

    for (; *next_pos != '\n' && *next_pos != '\0'; next_pos++);

    const char* tag_line_end = next_pos;

    for (; *next_pos == '\n' || *next_pos == ' '; next_pos++)
      if (*next_pos == '\n')
        lineno++;

    int line_offset = lineno;

      //split tokens in a line

    std::vector<std::string> tokens = common::split(std::string(tag_line_start, tag_line_end).c_str());

    engine_check(tokens.size() >= 2);

      //search the end of source

    const char* end_pos = strstr_with_line_numbers(next_pos, SHADER_PRAGMA_TAG, lineno);

      //remove the leading spaces

    if (end_pos)
    {
      for (;end_pos != next_pos && end_pos[-1] != '\n'; end_pos--);
    }
    else
    {
      end_pos = next_pos + strlen(next_pos);
    }

      //add new source

    std::string& shader_type = tokens[1];
    auto& source = sources[shader_type];

    source.source_code.assign(next_pos, end_pos);
    source.lineno = line_offset;

    if (!*end_pos)
      return nullptr;

    return end_pos;
  };

  for (const char* pos=source_code; pos; pos=parser(pos));

    //create shaders

  Shader vertex_shader = create_vertex_shader(common::format("vs.%s", name).c_str(), sources["vertex"].source_code.c_str(), sources["vertex"].lineno);
  Shader pixel_shader = create_pixel_shader(common::format("ps.%s", name).c_str(), sources["pixel"].source_code.c_str(), sources["pixel"].lineno);
  Program program = create_program(name, vertex_shader, pixel_shader);

  return program;
}

Program Device::create_program_from_file(const char* file_name)
{
  engine_check_null(file_name);

  std::string source_code = common::load_file_as_string(file_name);
  std::string name = notdir(basename(file_name).c_str());

  return create_program_from_source(name.c_str(), source_code.c_str());
}

Program Device::get_default_program() const
{
  return *impl->default_program;
}

Pass Device::create_pass(const Program& program)
{
  return Pass(impl->context, impl->window_frame_buffer, program);
}

Pass Device::create_pass()
{
  return create_pass(get_default_program());
}

Mesh Device::create_mesh(const media::geometry::Mesh& mesh, const MaterialList& materials)
{
  return Mesh(impl->context, mesh, materials);
}

Primitive Device::create_plane(const Material& material)
{
  Vertex vertices [] = {
    {math::vec3f(-1.f, -1.f, 0), math::vec3f(0.f, 1.f, 0.f), math::vec4f(1.f, 1.f, 1.f, 1.0f), math::vec2f(0, 0)},
    {math::vec3f(-1.f, 1.f, 0), math::vec3f(0.f, 1.f, 0.f), math::vec4f(1.f, 1.f, 1.f, 1.0f), math::vec2f(0, 1)},
    {math::vec3f(1.f, 1.f, 0), math::vec3f(0.f, 1.f, 0.f), math::vec4f(1.f, 1.f, 1.f, 1.0f), math::vec2f(1, 1)},
    {math::vec3f(1.f, -1.f, 0), math::vec3f(0.f, 1.f, 0.f), math::vec4f(1.f, 1.f, 1.f, 1.0f), math::vec2f(1, 0)},
  };
  IndexBuffer::index_type indices [] = {2, 1, 0, 3, 2, 0};

  VertexBuffer vertex_buffer = create_vertex_buffer(4);
  IndexBuffer index_buffer = create_index_buffer(6);

  vertex_buffer.set_data(0, 4, vertices);
  index_buffer.set_data(0, 6, indices);

  return Primitive(material, PrimitiveType::PrimitiveType_TriangleList, vertex_buffer, index_buffer, 0, 2, 0);
}

RenderBuffer Device::create_render_buffer(size_t width, size_t height, PixelFormat format)
{
  return RenderBuffer(impl->context, width, height, format);
}
