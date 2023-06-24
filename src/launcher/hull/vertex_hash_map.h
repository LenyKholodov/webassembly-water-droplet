#pragma once

#include <math/vector.h>

#include <unordered_map>

struct VectorHash
{
  size_t operator () (const math::vec3f& v) const
  {
    return size_t (v [0] + v [1] + v [2]);
  }
};

typedef std::unordered_map<math::vec3f, size_t> VertexHashMap;
