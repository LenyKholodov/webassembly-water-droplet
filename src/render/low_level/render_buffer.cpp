#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

/// Implementation details of render buffer
struct RenderBuffer::Impl
{
  DeviceContextPtr context; //device context
  GLuint render_buffer_id; //identifier of render buffer
  PixelFormat format; //pixel format

  Impl(const DeviceContextPtr& context, size_t width, size_t height, PixelFormat format)
    : context(context)
    , format(format)
  {
    engine_check(context);

    glGenRenderbuffers(1, &render_buffer_id);

    if (!render_buffer_id)
      throw Exception::format("Render buffer creation failed");

    try
    {
      glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id);

      GLenum gl_internal_format;

      switch(format)
      {
        case PixelFormat_RGBA8:
          gl_internal_format = GL_RGBA8;
          break;
        case PixelFormat_RGB16F:
          gl_internal_format = GL_RGB16F;
          break;
        case PixelFormat_D24:
          gl_internal_format = GL_DEPTH_COMPONENT;
          break;
        default:
          throw Exception::format("Invalid render buffer pixel format %d", format);
      }

      glRenderbufferStorage(GL_RENDERBUFFER, gl_internal_format, static_cast<GLint>(width), static_cast<GLint>(height));

      context->check_errors();
    }
    catch (...)
    {
      destroy();
      throw;
    }
  }

  ~Impl()
  {
    destroy();
  }

  void destroy()
  {
    if (!render_buffer_id)
      return;

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &render_buffer_id);

    render_buffer_id = 0;
  }

  void bind()
  {
    engine_check(context);

    glBindRenderbuffer(GL_RENDERBUFFER, render_buffer_id);

    context->check_errors();
  }
};

RenderBuffer::RenderBuffer(const DeviceContextPtr& context, size_t width, size_t height, PixelFormat format)
{
  engine_check(context);

  impl.reset(new Impl(context, width, height, format));
}

PixelFormat RenderBuffer::format() const
{
  return impl->format;
}

void RenderBuffer::get_info(RenderBufferInfo& out_info) const
{
  out_info.render_buffer_id = impl->render_buffer_id;
}

void RenderBuffer::bind() const
{
  impl->bind();
}
