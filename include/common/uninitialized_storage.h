#pragma once

#include <cstdlib>
#include <memory>

#include <common/exception.h>

namespace engine {
namespace common {

/// Uninitialized storage
template<class T>
class UninitializedStorage
{
  public:
    /// Constructors
    UninitializedStorage();
    UninitializedStorage(size_t size);

    /// No copy
    UninitializedStorage(const UninitializedStorage&) = delete;
    UninitializedStorage& operator =(const UninitializedStorage&) = delete;

    /// Get size
    size_t size();

    /// Get capacity
    size_t capacity();

    /// Get data
    const T* data() const;
    T* data();

    /// Resize
    void resize(size_t new_size);

    /// Change capacity
    void reserve(size_t new_capacity);

  private:
    std::unique_ptr<void*, decltype(&::free)> buffer;
    size_t buffer_size;
    size_t buffer_capacity;
};

/// Constructors
template<class T>
inline UninitializedStorage<T>::UninitializedStorage()
  : buffer(0, &::free)
  , buffer_size()
  , buffer_capacity()
  {}

template<class T>
inline UninitializedStorage<T>::UninitializedStorage(size_t size)
  : buffer(0, &::free)
  , buffer_size()
  , buffer_capacity()
{
  if (!size)
    return;

  resize(size);
}

/// Get data
template<class T>
inline size_t UninitializedStorage<T>::size()
{
  return buffer_size;
}

template<class T>
size_t UninitializedStorage<T>::capacity()
{
  return buffer_capacity;
}

template<class T>
const T* UninitializedStorage<T>::data() const
{
  return reinterpret_cast<T*>(buffer.get());
}

template<class T>
T* UninitializedStorage<T>::data()
{
  return reinterpret_cast<T*>(buffer.get());
}

/// Resize / reserve
template<class T>
void UninitializedStorage<T>::resize(size_t new_size)
{
  if (buffer_size == new_size)
    return;

  if (new_size > buffer_capacity)
    reserve(new_size);

  buffer_size = new_size;
}

template<class T>
void UninitializedStorage<T>::reserve(size_t new_capacity)
{
  if (new_capacity <= buffer_capacity)
    return;

  std::unique_ptr<void*, decltype(&::free)> new_buffer(static_cast<void**>(malloc(sizeof(T) * new_capacity)), &::free);

  if (!new_buffer)
    throw Exception("Can't allocate requested memory amount");

  if (buffer_size)
    memcpy(new_buffer.get(), buffer.get(), buffer_size * sizeof(T));

  buffer = std::move(new_buffer);

  buffer_capacity = new_capacity;
}

}}
