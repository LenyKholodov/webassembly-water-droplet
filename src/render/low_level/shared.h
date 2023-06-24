#pragma once

#include <render/device.h>
#include <application/window.h>
#include <media/image.h>
#include <common/exception.h>
#include <common/log.h>
#include <common/string.h>
#include <common/file.h>
#include <common/named_dictionary.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

#ifndef __EMSCRIPTEN__
extern "C"
{
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
}
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#endif

//#define CHECK_GL_ERRORS

#ifdef CHECK_GL_ERRORS
#warning "GL errors checking enabled"
#endif

namespace engine {
namespace render {
namespace low_level {

/// Basic class for internal render objects
class BaseObject
{
  public:
    BaseObject() {}

    /// Non-copyable pattern
    BaseObject(const BaseObject&) = delete;
    BaseObject(BaseObject&&) = delete;
    BaseObject& operator = (const BaseObject&) = delete;
    BaseObject& operator = (BaseObject&&) = delete;
};

/// Context capabilities
struct DeviceContextCapabilities
{
  uint32_t active_textures_count;

  DeviceContextCapabilities()
    : active_textures_count()
  {
  }
};

/// Device context implementation
class DeviceContextImpl: BaseObject
{
  public:
    /// Constructor
    DeviceContextImpl(const Window& window, const DeviceOptions& options);

    /// Destructor
    ~DeviceContextImpl();

    /// Window handle
    GLFWwindow* handle() const { return context; }

    /// Access to a render window
    const Window& window() const { return render_window; }

    /// Options
    const DeviceOptions& options() const { return device_options; }

    /// Context capabilities
    const DeviceContextCapabilities& capabilities() const { return device_capabilities; }

    /// Make context current
    void make_current()
    {
      make_current(context);
    }

    /// Check errors
    void check_errors()
    {
      if (!device_options.debug)
        return;

      check_errors_impl();
    }

    /// Clear all errors
    static void clear_errors()
    {
      while (glGetError () != GL_NO_ERROR);
    }    

  private:
    static void make_current(GLFWwindow* context)
    {
      static GLFWwindow* current_context = 0;

      if (current_context == context)
        return;

      engine_log_debug("glfwMakeContextCurrent(%p)", context);

      glfwMakeContextCurrent(context);

      if (current_context)
        check_errors_impl();

      current_context = context;
    }    

    static void check_errors_impl()
    {
#ifdef CHECK_GL_ERRORS
      using common::Exception;

      GLenum error = glGetError ();

      clear_errors();

      switch (error)
      {
        case GL_NO_ERROR:
          break;
        case GL_INVALID_ENUM:
          throw Exception::format("OpenGL error: invalid enum");
        case GL_INVALID_VALUE:
          throw Exception::format("OpenGL error: invalid value");
        case GL_INVALID_OPERATION:
          throw Exception::format("OpenGL error: invalid operation");
        case GL_STACK_OVERFLOW:
          throw Exception::format("OpenGL error: stack overflow");
        case GL_STACK_UNDERFLOW:
          throw Exception::format("OpenGL error: stack underflow");
        case GL_OUT_OF_MEMORY:
          throw Exception::format("OpenGL error: out of memory");
        case GL_INVALID_FRAMEBUFFER_OPERATION:
          throw Exception::format("OpenGL error: invalid framebuffer operation");
        default:
          throw Exception::format("OpenGL error: code=0x%04x", error);
      }
#endif
    }

  private:
    Window render_window; //target window
    GLFWwindow* context; //context
    DeviceOptions device_options; //device options
    DeviceContextCapabilities device_capabilities; //device context capabilities
};

/// Texture level info
struct TextureLevelInfo
{
  GLuint texture_id; //texture object
  GLenum target; //target
  GLint width; //layer width
  GLint height; //layer height

  TextureLevelInfo()
    : texture_id()
    , target()
    , width()
    , height()
  {
  }
};

/// Render buffer info
struct RenderBufferInfo
{
  GLuint render_buffer_id; //render buffer object

  RenderBufferInfo()
    : render_buffer_id()
  {
  }
};

/// Program parameter
struct ProgramParameter 
{
  std::string name; //parameter name
  common::StringHash name_hash; //hash of the name
  PropertyType type; //type of parameter
  size_t elements_count; //number of elements (for arrays)
  bool is_sampler; //is this parameter a sampler
  int location; //location of the parameter

  ProgramParameter()
    : type()
    , name_hash("")
    , elements_count()
    , is_sampler()
    , location(-1)
  { }
};

}}}
