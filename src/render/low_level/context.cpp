#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

namespace
{

const char* get_gl_debug_source(GLenum value)
{
  switch (value)
  {
    case GL_DEBUG_SOURCE_API:             return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "WindowSystem";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "ShaderCompiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:     return "ThirdParty";
    case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
    default:
    case GL_DEBUG_SOURCE_OTHER:           return "Other";
   }
}

const char* get_gl_debug_type(GLenum value)
{
  switch (value)
  {
    case GL_DEBUG_TYPE_ERROR:               return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DeprecatedBehaviour";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "UndefinedBehaviour";
    case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
    case GL_DEBUG_TYPE_MARKER:              return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP:          return "PushGroup";
    case GL_DEBUG_TYPE_POP_GROUP:           return "PopGroup";
    default:
    case GL_DEBUG_TYPE_OTHER:               return "Other";
  }
}

const char* get_gl_debug_severity(GLenum value)
{
  switch (value)
  {
    case GL_DEBUG_SEVERITY_HIGH:         return "high";
    case GL_DEBUG_SEVERITY_MEDIUM:       return "medium";
    case GL_DEBUG_SEVERITY_LOW:          return "low";
    default:
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "info";
  }
}

void APIENTRY glDebugOutput(GLenum source, 
                            GLenum type, 
                            unsigned int id, 
                            GLenum severity, 
                            GLsizei length, 
                            const char *message, 
                            const void *userParam)
{
    // ignore non-significant error/warning codes

  switch (id)
  {
    case 131169:
    case 131185:
    case 131218:
    case 131204:
      return;
    default:
      break;
  }

  const char* gl_source = get_gl_debug_source(source);
  const char* gl_type = get_gl_debug_type(type);
  const char* gl_severity = get_gl_debug_severity(severity);

  engine_log_debug("OpenGL %15s %20s (%5s): id=%06u: %s", gl_source, gl_type, gl_severity, id, message);
}

}

DeviceContextImpl::DeviceContextImpl(const Window& window, const DeviceOptions& options)
  : render_window(window)
  , context(window.handle())
  , device_options(options)
{
  engine_log_info("Initializing OpenGL context...");

#ifdef CHECK_GL_ERRORS
  engine_log_warning("GL error checking is enabled!!!");
#endif 

  make_current();

  engine_log_info("...loading OpenGL functions");

#ifndef __EMSCRIPTEN__
  if (!gladLoadGL(glfwGetProcAddress))
    throw Exception::format("gladLoadGL failed");
#endif

  if (options.vsync)
  {
    engine_log_info("...enabling VSync");
    glfwSwapInterval(1);
  }

    //GL info dump

  const char* version_string    = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* vendor_string     = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const char* renderer_string   = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

  engine_log_info("...OpenGL version:    %s", version_string);
  engine_log_info("...OpenGL vendor:     %s", vendor_string);
  engine_log_info("...OpenGL renderer:   %s", renderer_string);
  engine_log_info("...OpenGL extensions:");

  const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

  engine_check(extensions);

  std::string extensions_string = extensions;

  for (size_t pos = 0; ; )
  {
    size_t next_pos = extensions_string.find(' ', pos);

    if (next_pos == std::string::npos)
      next_pos = extensions_string.size();

    engine_log_info("......%s", extensions_string.substr(pos, next_pos - pos).c_str());

    if (next_pos == extensions_string.size())
      break;

    pos = next_pos + 1;
  }

    //enabling debug output

#ifndef __EMSCRIPTEN__
  if (options.debug && glDebugMessageCallback && glDebugMessageControl)
  {
    engine_log_info("...enabling OpenGL debug output");
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  }
#endif

    //get capabilities

  GLint texture_units_count = 0;

  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, (GLint*)&texture_units_count);

  engine_check(texture_units_count >= 8);

  device_capabilities.active_textures_count = texture_units_count;

  check_errors();
}

DeviceContextImpl::~DeviceContextImpl()
{
  try
  {
    engine_log_info("Destroying OpenGL context...");

    make_current(nullptr);
  }
  catch (...)
  {
    //ignore all exceptions on destruction
  }
}
