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

  BufferImpl(const DeviceContextPtr& context, GLenum target, size_t count, size_t element_size)
    : context(context)
    , count(count)
    , element_size(element_size)
    , target(target)
    , vbo_id()
  {
    engine_check(context);

    context->make_current();

      //create VBO

    glGenBuffers(1, &vbo_id);

    context->check_errors();

    engine_check(vbo_id);

    bind();

      //allocate buffer

    const GLenum usage_mode = GL_STATIC_DRAW; //TODO: add as parameter

    glBufferData(target, count * element_size, nullptr, usage_mode); 

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

  void bind()
  {
    context->make_current();

    glBindBuffer(target, vbo_id);

    context->check_errors();
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
