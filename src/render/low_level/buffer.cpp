#include "shared.h"

using namespace engine::render::low_level;
using namespace engine::common;

/// Implementation details of buffer
struct engine::render::low_level::BufferImpl
{ 
  DeviceContextPtr context; //device context
  size_t count; //number of elements
  size_t element_size; //size of one element
  GLenum target; //buffer target
  GLuint vbo_id; //vertex buffer object
  GLenum usage; //GL usage hint (GL_STATIC_DRAW by default; switched to GL_DYNAMIC_DRAW for streamed buffers)

  BufferImpl(const DeviceContextPtr& context, GLenum target, size_t count, size_t element_size)
    : context(context)
    , count(count)
    , element_size(element_size)
    , target(target)
    , vbo_id()
    , usage(GL_STATIC_DRAW)
  {
    engine_check(context);

    context->make_current();

      //create VBO

    glGenBuffers(1, &vbo_id);

    context->check_errors();

    engine_check(vbo_id);

    bind();

      //allocate buffer

    glBufferData(target, count * element_size, nullptr, usage);

    context->check_errors();
  }

  ~BufferImpl()
  {
    try
    {
      context->make_current();

      glBindBuffer(target, 0);
      glDeleteBuffers(1, &vbo_id);
    }
    catch (...)
    {
      //ignore all exceptions in descructor
    }
  }

  void set_data(size_t offset, size_t count, const void* data)
  {
    bind();

    glBufferSubData(target, offset * element_size, count * element_size, data);

    context->check_errors();
  }

  // Switch the buffer's GL usage hint (re-allocates/orphans the store). Used to promote a
  // per-frame-streamed buffer (e.g. the animated water surface) from STATIC_DRAW to DYNAMIC_DRAW
  // so the driver double-buffers it instead of stalling on every glBufferSubData.
  void set_usage(GLenum new_usage)
  {
    if (usage == new_usage)
      return;

    bind();

    glBufferData(target, count * element_size, nullptr, new_usage);

    context->check_errors();

    usage = new_usage;
  }

  void bind()
  {
    context->make_current();

    glBindBuffer(target, vbo_id);

    context->check_errors();
  }

  void resize(size_t new_count)
  {
    if (new_count == count)
      return;

    context->make_current();

    glBindBuffer(target, vbo_id);

    engine_log_debug("resize buffer %u -> %u; elsize=%u", count, new_count, element_size);

    glBufferData(target, new_count * element_size, nullptr, usage); // keep whatever usage hint was set (static or dynamic)

    context->check_errors();

    count = new_count;
  }
};

///
/// VertexBuffer
///

VertexBuffer::VertexBuffer(const DeviceContextPtr& context, size_t vertices_count)
  : impl(std::make_shared<BufferImpl>(context, GL_ARRAY_BUFFER, vertices_count, sizeof (Vertex)))
{
}

size_t VertexBuffer::vertices_count() const
{
  return impl->count;
}

void VertexBuffer::set_data(size_t offset, size_t count, const Vertex* vertices)
{
  impl->set_data(offset, count, vertices);
}

void VertexBuffer::bind() const
{
  impl->bind();
}

void VertexBuffer::resize(size_t new_count)
{
  impl->resize(new_count);
}

void VertexBuffer::ensure_dynamic()
{
  impl->set_usage(GL_DYNAMIC_DRAW);
}

///
/// IndexBuffer
///

IndexBuffer::IndexBuffer(const DeviceContextPtr& context, size_t indices_count)
  : impl(std::make_shared<BufferImpl>(context, GL_ELEMENT_ARRAY_BUFFER, indices_count, sizeof(index_type)))
{
}

size_t IndexBuffer::indices_count() const
{
  return impl->count;
}

void IndexBuffer::set_data(size_t offset, size_t count, const index_type* indices)
{
  impl->set_data(offset, count, indices);
}

void IndexBuffer::bind() const
{
  impl->bind();
}

void IndexBuffer::resize(size_t new_count)
{
  impl->resize(new_count);
}
