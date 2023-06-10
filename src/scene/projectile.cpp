#include "shared.h"

#include <scene/projectile.h>

#include <common/exception.h>

#include <string>

using namespace engine::common;
using namespace engine::scene;

///
/// Projectile
///

/// Implementation details of Projectile
struct Projectile::Impl
{
  std::string image; //image name
  math::vec3f color; //projectile color
  float intensity; //projectile intensity
  math::mat4f projection_matrix;  //projection matrix
  bool is_projection_matrix_dirty; //is projection matrix needs update

  Impl()
    : color(1.0f)
    , intensity(1.0f)
    , projection_matrix(1.0f)
    , is_projection_matrix_dirty(true)
  {
  }
};

Projectile::Projectile()
  : impl(std::make_unique<Impl>())
{
}

void Projectile::set_image(const char* name)
{
  engine_check_null(name);
  impl->image = name;
}

void Projectile::set_color(const math::vec3f& color)
{
  impl->color = color;
}

const math::vec3f& Projectile::color() const
{
  return impl->color;
}

void Projectile::set_intensity(float intensity)
{
  impl->intensity = intensity;
}

float Projectile::intensity() const
{
  return impl->intensity;
}

const char* Projectile::image() const
{
  return impl->image.c_str();
}

const math::mat4f& Projectile::projection_matrix() const
{
  if (!impl->is_projection_matrix_dirty)
    return impl->projection_matrix;

  const_cast<Projectile&>(*this).recompute_projection_matrix();

  impl->is_projection_matrix_dirty = false;

  return impl->projection_matrix;
}

void Projectile::set_projection_matrix(const math::mat4f& tm)
{
  impl->projection_matrix = tm;
  impl->is_projection_matrix_dirty = false;
}

void Projectile::invalidate_projection_matrix()
{
  impl->is_projection_matrix_dirty = true;
}

void Projectile::visit(ISceneVisitor& visitor)
{
  Node::visit(visitor);

  visitor.visit(*this);
}

///
/// Perspective Projectile
///

/// Implementation details of PerspectiveProjectile
struct PerspectiveProjectile::Impl
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

PerspectiveProjectile::PerspectiveProjectile()
  : impl(std::make_unique<Impl>())
{
}

PerspectiveProjectile::Pointer PerspectiveProjectile::create()
{
  return PerspectiveProjectile::Pointer(new PerspectiveProjectile);
}

void PerspectiveProjectile::recompute_projection_matrix()
{
  set_projection_matrix(compute_perspective_proj_tm(impl->fov_x, impl->fov_y, impl->z_near, impl->z_far));
}

void PerspectiveProjectile::set_fov_x(const math::anglef& fov_x)
{
  impl->fov_x = fov_x;
  invalidate_projection_matrix();
}

void PerspectiveProjectile::set_fov_y(const math::anglef& fov_y)
{
  impl->fov_y = fov_y;
  invalidate_projection_matrix();
}

void PerspectiveProjectile::set_z_near(float z_near)
{
  impl->z_near = z_near;
  invalidate_projection_matrix();
}

void PerspectiveProjectile::set_z_far(float z_far)
{
  impl->z_far = z_far;
  invalidate_projection_matrix();
}

const math::anglef& PerspectiveProjectile::fov_x() const
{
  return impl->fov_x;
}

const math::anglef& PerspectiveProjectile::fov_y() const
{
  return impl->fov_y;
}

float PerspectiveProjectile::z_near() const
{
  return impl->z_near;
}

float PerspectiveProjectile::z_far() const
{
  return impl->z_far;
}

void PerspectiveProjectile::visit(ISceneVisitor& visitor)
{
  Projectile::visit(visitor);

  visitor.visit(*this);
}
