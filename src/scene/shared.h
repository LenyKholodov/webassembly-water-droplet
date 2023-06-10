#pragma once

#include <common/exception.h>

#include <math/angle.h>
#include <math/matrix.h>

namespace engine {
namespace scene {

/// compute perspective matrix
inline math::mat4f compute_perspective_proj_tm(
  const math::anglef& fov_x,
  const math::anglef& fov_y,
  float z_near,
  float z_far)
{
  float width  = 2.f * tan(fov_x * 0.5f) * z_near,
        height = 2.f * tan(fov_y * 0.5f) * z_near,
        depth  = z_far - z_near;

  static constexpr float EPS = 1e-6f;        

  engine_check(fabs(width) >= EPS);
  engine_check(fabs(height) >= EPS);
  engine_check(fabs(depth) >= EPS);

  math::mat4f tm;

  tm[0] = math::vec4f(-2.0f * z_near / width, 0, 0, 0);
  tm[1] = math::vec4f(0, 2.0f * z_near / height, 0, 0);
  tm[2] = math::vec4f(0, 0, (z_far + z_near) / depth, -2.0f * z_near * z_far / depth);
  tm[3] = math::vec4f(0, 0, 1, 0);

  return tm;
}

}}
