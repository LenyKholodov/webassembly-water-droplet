#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

///
/// Constants
///
static constexpr size_t MAX_COLOR_TARGETS_COUNT = 8; //max number of color targets

namespace
{

/// Render target type
enum RenderTargetType
{
  RenderTargetType_Window,
  RenderTargetType_Texture2D,
  RenderTargetType_RenderBuffer,  
};

/// Render target of frame buffer
struct RenderTarget
{
  RenderTargetType type; //type of the target
  std::unique_ptr<Texture> texture; //texture
  std::unique_ptr<RenderBuffer> render_buffer; //render_buffer
  bool is_colored; //is this color render target
  size_t mip_level; //mip level for rendering
  TextureLevelInfo level_info; //texture level info
  RenderBufferInfo render_buffer_info; //texture object
  GLenum attachment; //attachment for this target

  RenderTarget()
    : type(RenderTargetType_Window)
    , is_colored(true)
    , mip_level()
    , attachment(GL_BACK) //TODO: front/back window buffer rendering support
  {
  }

  RenderTarget(const Texture& in_texture, size_t layer, size_t mip_level, size_t render_target_index)
    : type(RenderTargetType_Texture2D)
    , is_colored()
    , mip_level(mip_level)
    , attachment()
  {
    engine_check_range(layer, in_texture.layers());
    engine_check_range(mip_level, in_texture.mips_count());

    switch (in_texture.format())
    {
      case PixelFormat_RGBA8:
      case PixelFormat_RGB16F:
        is_colored = true;
        attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + render_target_index);
        break;
      default:
        engine_check(render_target_index == 0);
        attachment = GL_DEPTH_ATTACHMENT;
        break;
    }

    in_texture.get_level_info(layer, mip_level, level_info);

    texture = std::make_unique<Texture>(in_texture);
  }  

  RenderTarget(const RenderBuffer& in_render_buffer, size_t render_target_index)
    : type(RenderTargetType_RenderBuffer)
    , is_colored()
    , mip_level()
    , attachment()
  {
    switch (in_render_buffer.format())
    {
      case PixelFormat_RGBA8:
      case PixelFormat_RGB16F:
        is_colored = true;
        attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + render_target_index);
        break;
      default:
        engine_check(render_target_index == 0);
        attachment = GL_DEPTH_ATTACHMENT;
        break;
    }

    in_render_buffer.get_info(render_buffer_info);

    render_buffer = std::make_unique<RenderBuffer>(in_render_buffer);
  }
};

typedef std::vector<RenderTarget> RenderTargetArray;

}

/// Implementation details of device
struct FrameBuffer::Impl
{
  DeviceContextPtr context; //device context
  GLuint frame_buffer_id; //identifier of frame buffer
  RenderTargetArray color_targets; //rendering targets list
  std::unique_ptr<RenderTarget> depth_stencil_target; //depth-stencil target
  Viewport viewport; //viewport for the buffer (TODO: multi-target viewport support)  
  bool need_reconfigure; //FBO needs to reconfigure

  Impl(const DeviceContextPtr& context, bool is_default)
    : context(context)
    , frame_buffer_id()
    , need_reconfigure(false)
  {
    engine_check(context);

    color_targets.reserve(MAX_COLOR_TARGETS_COUNT);

      //bind the context

    context->make_current();

      //generate new buffer ID

    if (is_default)
    {
      color_targets.emplace_back(RenderTarget());
    }
    else
    {
      need_reconfigure = true;
    }
  }

  ~Impl()
  {
    destroy();
  }

  Viewport get_default_viewport() const
  {
    const Window& window = context->window();
    int width = window.frame_buffer_width();
    int height = window.frame_buffer_height();

    return Viewport(0, 0, width, height);
  }

  void bind()
  {
    engine_check(context);

    if (need_reconfigure)
    {
      reconfigure();
    }
    else
    {
      glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
    }

    context->check_errors();
  }

  void destroy()
  {
    if (!frame_buffer_id)
      return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &frame_buffer_id);

    frame_buffer_id = 0;
  }

  void reconfigure()
  {
    destroy();

    if (color_targets.size() == 1 && color_targets.front().type == RenderTargetType_Window)
    {
      need_reconfigure = false;
      return;
    }

    glGenFramebuffers(1, &frame_buffer_id);
    
    if (!frame_buffer_id)
      throw Exception::format("FBO creation failed");

    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);  

    try
    {
        //add color targets

      for (auto& rt : color_targets)
      {
        switch (rt.type)
        {
          case RenderTargetType_Texture2D:
          {
            Texture& texture = *rt.texture;

            engine_check(&texture);
            engine_check(rt.is_colored);

            glFramebufferTexture2D(GL_FRAMEBUFFER, rt.attachment, rt.level_info.target,
              rt.level_info.texture_id, static_cast<GLint>(rt.mip_level));

            break;
          }
          case RenderTargetType_Window:
            throw Exception::format("Can't render both to window and texture simultaneously");
          default:
            unimplemented();
        }
      }

      if (depth_stencil_target)
      {
        engine_check(!depth_stencil_target->is_colored);

        switch (depth_stencil_target->type)
        {
          case RenderTargetType_Texture2D:
          {
            Texture& texture = *depth_stencil_target->texture;

            engine_check(&texture);

            glFramebufferTexture2D(GL_FRAMEBUFFER, depth_stencil_target->attachment, depth_stencil_target->level_info.target,
                                   depth_stencil_target->level_info.texture_id, static_cast<GLint>(depth_stencil_target->mip_level));

            break;
          }
          case RenderTargetType_RenderBuffer:
          {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, depth_stencil_target->attachment, GL_RENDERBUFFER, depth_stencil_target->render_buffer_info.render_buffer_id);
            break;
          }
          default:
            unimplemented();
        }
      }

        //check FBO status

      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

      check_frame_buffer_status(status);

      context->check_errors();
    }
    catch (...)
    {
      destroy();
      throw;
    }

    need_reconfigure = false;
  }

  static void check_frame_buffer_status(GLenum status)
  {
    switch (status)
    {
      case GL_FRAMEBUFFER_COMPLETE:
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        throw Exception::format("OpenGL bad framebuffer status: incomplete attachment");
      case GL_FRAMEBUFFER_UNSUPPORTED:
        throw Exception::format("OpenGL bad framebuffer status: unsupported framebuffer format");
      case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        throw Exception::format("OpenGL bad framebuffer status: missing attachment");
      case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        throw Exception::format("OpenGL bad framebuffer status: missing draw buffer");
      case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        throw Exception::format("OpenGL bad framebuffer status: missing read buffer");
      default:
        throw Exception::format("OpenGL bad framebuffer status: 0x%04x", status);
    }
  }
};

FrameBuffer::FrameBuffer(const DeviceContextPtr& context)
  : impl(new Impl(context, false))
{
}

FrameBuffer::FrameBuffer(const DeviceContextPtr& context, const Window& window)
{
  engine_check(context);
  engine_check(context->handle() == window.handle());

  impl.reset(new Impl(context, true));
}

void FrameBuffer::set_viewport(const Viewport& viewport)
{
  impl->viewport = viewport;
}

void FrameBuffer::reset_viewport()
{
  if (!impl->frame_buffer_id)
  {
      //for window based render targets just use window bounds

    impl->viewport = impl->get_default_viewport();
  }
  else
  {
    engine_check(impl->color_targets.size() > 0);

    impl->bind(); //for reconfiguring

    const RenderTarget& render_target = impl->color_targets.front();

    impl->viewport = Viewport(0, 0, render_target.level_info.width, render_target.level_info.height);
  }
}

const Viewport& FrameBuffer::viewport() const
{
  return impl->viewport;
}

void FrameBuffer::bind() const
{
  impl->bind();

  const Viewport& v = impl->viewport;

  glViewport(v.x, v.y, v.width, v.height);

    //configure MRT

  unsigned int attachments[MAX_COLOR_TARGETS_COUNT];
  unsigned int attachments_count = 0;

  for (const auto& rt : impl->color_targets)
  {
    attachments[attachments_count++] = rt.attachment;
  }

  switch (attachments_count)
  {
    case 0:
      glDrawBuffer(GL_NONE);
      break;
    case 1:
      glDrawBuffer(attachments[0]);
      break;
    default:
      glDrawBuffers(attachments_count, attachments);
      break;
  }

    //check errors

  impl->context->check_errors();
}

size_t FrameBuffer::color_targets_count() const
{
  return impl->color_targets.size();
}

void FrameBuffer::attach_color_target(const Texture& texture, size_t layer, size_t mip_level)
{
  engine_check(impl->color_targets.size() < MAX_COLOR_TARGETS_COUNT);

  RenderTarget new_target(texture, layer, mip_level, impl->color_targets.size());

  engine_check(new_target.is_colored);

  if (!impl->color_targets.empty())
  {
    size_t buffer_width = impl->color_targets.front().level_info.width;    
    size_t buffer_height = impl->color_targets.front().level_info.height;

    engine_check(buffer_width == new_target.level_info.width);
    engine_check(buffer_height == new_target.level_info.height);
  }

  impl->color_targets.emplace_back(std::move(new_target));

  impl->need_reconfigure = true;  
}

void FrameBuffer::attach_color_target(const RenderBuffer& render_buffer)
{
  unimplemented();
}

void FrameBuffer::detach_all_color_targets()
{
  impl->color_targets.clear();

  impl->need_reconfigure = true;  
}

void FrameBuffer::attach_depth_buffer(const Texture& texture, size_t layer, size_t mip_level)
{
  engine_check(!impl->depth_stencil_target);

  std::unique_ptr<RenderTarget> new_target = std::make_unique<RenderTarget>(texture, layer, mip_level, 0);

  engine_check(!new_target->is_colored);

  impl->depth_stencil_target.swap(new_target);

  impl->need_reconfigure = true;
}

void FrameBuffer::attach_depth_buffer(const RenderBuffer& render_buffer)
{
  engine_check(!impl->depth_stencil_target);

  std::unique_ptr<RenderTarget> new_target = std::make_unique<RenderTarget>(render_buffer, 0);

  engine_check(!new_target->is_colored);

  impl->depth_stencil_target.swap(new_target);

  impl->need_reconfigure = true;
}

void FrameBuffer::detach_depth_buffer()
{
  unimplemented();
}
