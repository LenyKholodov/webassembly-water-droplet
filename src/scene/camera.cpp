#include "shared.h"

#include <scene/camera.h>

#include <common/exception.h>

using namespace engine::common;
using namespace engine::scene;

///
/// Camera
///

/// Implementation details of Camera
struct Camera::Impl
{
  math::mat4f projection_matrix;  //projection matrix
  bool is_projection_matrix_dirty; //is projection matrix needs update

  Impl()
    : projection_matrix(1.0f)
    , is_projection_matrix_dirty(true)
  {
  }
};

Camera::Camera()
  : impl(std::make_unique<Impl>())
{
}

const math::mat4f& Camera::projection_matrix() const
{
  if (!impl->is_projection_matrix_dirty)
    return impl->projection_matrix;

  const_cast<Camera&>(*this).recompute_projection_matrix();

  impl->is_projection_matrix_dirty = false;

  return impl->projection_matrix;
}

void Camera::set_projection_matrix(const math::mat4f& tm)
{
  impl->projection_matrix = tm;
  impl->is_projection_matrix_dirty = false;
}

void Camera::invalidate_projection_matrix()
{
  impl->is_projection_matrix_dirty = true;
}

void Camera::visit(ISceneVisitor& visitor)
{
  Node::visit(visitor);

  visitor.visit(*this);
}

///
/// Perspective camera
///

/// Implementation details of PerspectiveCamera
struct PerspectiveCamera::Impl
{
  math::anglef fov_x; //fov x
  math::anglef fov_y; //fov y
  float z_near; //z near plane
  float z_far; //z far plane

  Impl()
    : fov_x(math::degree(90.f))
    , fov_y(math::degree(90.f))
    , z_near(0.f)
    , z_far(1.f)
  {
  }
};

PerspectiveCamera::PerspectiveCamera()
  : impl(std::make_unique<Impl>())
{
}

PerspectiveCamera::Pointer PerspectiveCamera::create()
{
  return PerspectiveCamera::Pointer(new PerspectiveCamera);
}

void PerspectiveCamera::recompute_projection_matrix()
{
  set_projection_matrix(compute_perspective_proj_tm(impl->fov_x, impl->fov_y, impl->z_near, impl->z_far));
}

void PerspectiveCamera::set_fov_x(const math::anglef& fov_x)
{
  impl->fov_x = fov_x;
  invalidate_projection_matrix();
}

void PerspectiveCamera::set_fov_y(const math::anglef& fov_y)
{
  impl->fov_y = fov_y;
  invalidate_projection_matrix();
}

void PerspectiveCamera::set_z_near(float z_near)
{
  impl->z_near = z_near;
  invalidate_projection_matrix();
}

void PerspectiveCamera::set_z_far(float z_far)
{
  impl->z_far = z_far;
  invalidate_projection_matrix();
}

const math::anglef& PerspectiveCamera::fov_x() const
{
  return impl->fov_x;
}

const math::anglef& PerspectiveCamera::fov_y() const
{
  return impl->fov_y;
}

float PerspectiveCamera::z_near() const
{
  return impl->z_near;
}

float PerspectiveCamera::z_far() const
{
  return impl->z_far;
}

void PerspectiveCamera::visit(ISceneVisitor& visitor)
{
  Camera::visit(visitor);

  visitor.visit(*this);
}
