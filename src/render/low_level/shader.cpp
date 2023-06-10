#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

///
/// Shader internals
///

struct engine::render::low_level::ShaderImpl
{
  DeviceContextPtr context; //context
  ShaderType type; //shader type
  std::string name; //shader name
  GLuint shader_id; //shader ID

  ShaderImpl(const DeviceContextPtr& context, ShaderType type, const char* name, const char* source_code)
    : context(context)
    , type(type)
    , name(name)
    , shader_id()  
  {
    engine_check(context);

      //create new shader

    context->make_current();

    GLenum gl_type = GL_NONE;
    const char* shader_type_string = "";

    switch (type)
    {
      case ShaderType_Vertex:
        gl_type = GL_VERTEX_SHADER;
        shader_type_string = "vertex";
        break;
      case ShaderType_Pixel:
        gl_type = GL_FRAGMENT_SHADER;
        shader_type_string = "pixel";
        break;
      default:
        throw Exception::format("Unexpected shader type %d", type);
    }

    engine_log_info("Compiling %s shader %s...", shader_type_string, this->name.c_str());

    shader_id = glCreateShader(gl_type);

      //compile shader

    const char* sources[] = {source_code};
    GLint sources_length[] = {(int)strlen(source_code)};

    glShaderSource(shader_id, 1, sources, sources_length);
    glCompileShader(shader_id);

      //check status

    GLint compile_status = 0;

    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);

      //dump logs

    GLint log_length = 0;

    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    if (log_length)
    {
      std::string log_buffer;

      log_buffer.resize(log_length - 1);

      GLsizei real_log_size = 0;

      glGetShaderInfoLog(shader_id, log_length, &real_log_size, &log_buffer[0]);

      if (real_log_size)
        log_buffer.resize(real_log_size - 1);

      engine_log_info("%s", log_buffer.c_str());

      if (!compile_status)
        throw Exception::format("Shader '%s' compilation error", this->name.c_str());

      context->check_errors();
    }
  }

  ~ShaderImpl()
  {
    try
    {
      context->make_current();
      glDeleteShader(shader_id);
    }
    catch (...)
    {
      //ignore all exceptions in descructor
    }
  }
};

///
/// Shader
///

Shader::Shader(const DeviceContextPtr& context, ShaderType type, const char* name, const char* source_code)
{
  engine_check_null(context);
  engine_check_null(name);
  engine_check_null(source_code);

  impl = std::make_shared<ShaderImpl>(context, type, name, source_code);
}

const char* Shader::name() const
{
  return impl->name.c_str();
}

ShaderType Shader::type() const
{
  return impl->type;
}

ShaderImpl& Shader::get_impl() const
{
  return *impl;
}

///
/// Program
///

typedef std::vector<ProgramParameter> ProgramParameterArray;

struct Program::Impl
{
  DeviceContextPtr context; //device context
  Shader vertex_shader; //vertex shader
  Shader pixel_shader; //pixel shader
  std::string name; //program name
  GLuint program_id; //GL program ID
  ProgramParameterArray parameters;

  Impl(const DeviceContextPtr& context, const char* name, const Shader& vertex_shader, const Shader& pixel_shader)
    : context(context)
    , vertex_shader(vertex_shader)
    , pixel_shader(pixel_shader)
    , name(name)
    , program_id()
  {
    context->make_current();

      //create program

    engine_log_info("Linking shader program %s...", this->name.c_str());

    program_id = glCreateProgram();

    if (!program_id)
      throw Exception::format("glCreateProgram failed");

      //link program

    glAttachShader(program_id, vertex_shader.get_impl().shader_id);
    glAttachShader(program_id, pixel_shader.get_impl().shader_id);
    glLinkProgram(program_id);

      //check status

    GLint link_status = 0;

    glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);

      //dump logs

    GLint log_length = 0;

    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
  
    if (log_length)
    {
      std::string log_buffer;

      log_buffer.resize(log_length - 1);

      GLsizei real_log_size = 0;

      glGetProgramInfoLog(program_id, log_length, &real_log_size, &log_buffer[0]);

      if (real_log_size)
        log_buffer.resize(real_log_size - 1);

      engine_log_info("%s", log_buffer.c_str());
    }

      //get parameters

    GLint parameters_count = 0, max_parameter_name_length = 0;

    glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &parameters_count);
    glGetProgramiv(program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_parameter_name_length);

    parameters.reserve(size_t(parameters_count));

    std::string parameter_name;

    for (GLint i=0; i<parameters_count; i++)
    {
        //read parameter name

      parameter_name.resize(max_parameter_name_length);      
      
      GLint  name_length = 0, elements_count = 0;
      GLenum type = 0;
      
      glGetActiveUniform(program_id, i, (unsigned int)parameter_name.size(), &name_length, &elements_count, &type, &parameter_name[0]);

      if ((size_t)name_length > parameter_name.size())
        name_length = (unsigned int)parameter_name.size ();
        
      if (name_length < 0)
        name_length = 0;
        
      parameter_name.resize(name_length);

      if (strstr(parameter_name.c_str(), "[0]") == &*parameter_name.end() - 3)
        parameter_name.resize(parameter_name.size () - 3);

        //prepare parameter
     
      ProgramParameter parameter;
      
      parameter.name           = parameter_name;
      parameter.name_hash      = common::StringHash(parameter_name);
      parameter.elements_count = (unsigned int)elements_count;
      parameter.is_sampler     = false;
      parameter.location       = glGetUniformLocation(program_id, parameter_name.c_str());
      
      switch (type)
      {
        case GL_INT:
          parameter.type = parameter.elements_count <= 1 ? PropertyType_Int : PropertyType_IntArray;
          break;
        case GL_FLOAT:
          parameter.type = parameter.elements_count <= 1 ? PropertyType_Float : PropertyType_FloatArray;
          break;
        case GL_FLOAT_VEC2:
          parameter.type = parameter.elements_count <= 1 ? PropertyType_Vec2f : PropertyType_Vec2fArray;
          break;
        case GL_FLOAT_VEC3:
          parameter.type = parameter.elements_count <= 1 ? PropertyType_Vec3f : PropertyType_Vec3fArray;
          break;
        case GL_FLOAT_VEC4:
          parameter.type = parameter.elements_count <= 1 ? PropertyType_Vec4f : PropertyType_Vec4fArray;
          break;
        case GL_FLOAT_MAT4:
          parameter.type = parameter.elements_count <= 1 ? PropertyType_Mat4f : PropertyType_Mat4fArray;
          break;
        case GL_SAMPLER_2D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_1D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_2D_RECT:
        case GL_SAMPLER_2D_RECT_SHADOW:
          parameter.is_sampler = true;
          parameter.type = PropertyType_Int;
          break;                
        default:
          throw Exception::format("Unknown uniform '%s' in program '%s' gl_type 0x%04x with %u element(s)",
            parameter.name.c_str (), name, type, elements_count);
      }

      if (elements_count > 1)
      {
        engine_log_debug("...%03d: uniform '%s' type %s[%u] (gl_type=0x%04x)",
          parameter.location, parameter.name.c_str(), Property::get_type_name(parameter.type), elements_count, type);
      }
      else
      {
        engine_log_debug("...%03d: uniform '%s' type %s (gl_type=0x%04x)",
          parameter.location, parameter.name.c_str(), Property::get_type_name(parameter.type), type);
      }

      parameters.emplace_back(std::move(parameter));
    }

      //check errors

    context->check_errors();
  }

  ~Impl()
  {
    try
    {
      context->make_current();

      glDetachShader(program_id, vertex_shader.get_impl().shader_id);
      glDetachShader(program_id, pixel_shader.get_impl().shader_id);
      glDeleteProgram(program_id);
    }
    catch (...)
    {
      //ignore all exceptions in descructor
    }    
  }
};

Program::Program(const DeviceContextPtr& context, const char* name, const Shader& vertex_shader, const Shader& pixel_shader)
{
  engine_check_null(name);
  engine_check_null(context);

  impl = std::make_shared<Impl>(context, name, vertex_shader, pixel_shader);
}

const char* Program::name() const
{
  return impl->name.c_str();
}

int Program::find_uniform_location(const char* name) const
{
  if (!name)
    return -1;

  impl->context->make_current();

  return glGetUniformLocation(impl->program_id, name);
}

int Program::find_attribute_location(const char* name) const
{
  if (!name)
    return -1;

  impl->context->make_current();

  return glGetAttribLocation(impl->program_id, name);
}

int Program::get_uniform_location(const char* name) const
{
  int location = find_uniform_location(name);

  if (location >= 0)
    return location;

  throw Exception::format("Unresolved shader program '%s' uniform '%s'",
    impl->name.c_str(), name); 
}

int Program::get_attribute_location(const char* name) const
{
  int location = find_attribute_location(name);

  if (location >= 0)
    return location;

  throw Exception::format("Unresolved shader program '%s' attribute '%s'",
    impl->name.c_str(), name);
}

void Program::bind() const
{
  impl->context->make_current();

  glUseProgram(impl->program_id);
}

size_t Program::parameters_count() const
{
  return impl->parameters.size();
}

const ProgramParameter* Program::parameters() const
{
  if (impl->parameters.empty())
    return nullptr;

  return &impl->parameters[0];
}
