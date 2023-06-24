#pragma once

#include <scene/node.h>

#include <math/angle.h>

namespace engine {
namespace scene {

const float DEFAULT_PROJECTILE_RANGE = 1e9;

/// Projectile
class Projectile : public Node
{
  public: 
    typedef std::shared_ptr<Projectile> Pointer;
  
    /// Color multiplier
    const math::vec3f& color() const;    

    /// Set color multipler
    void set_color(const math::vec3f& color);
    
    /// Intensity
    float intensity() const;

    /// Set intensity
    void set_intensity(float value);

    /// Set image name
    void set_image(const char* name);

    /// Image name
    const char* image() const;

    /// Projection matrix
    const math::mat4f& projection_matrix() const;

  protected:
    /// Constructor
    Projectile();

    /// Set projection matrix
    void set_projection_matrix(const math::mat4f&);

    /// Projection matrix needs to be updated
    void invalidate_projection_matrix();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    virtual void recompute_projection_matrix() = 0;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Perspective projectile
class PerspectiveProjectile : public Projectile
{
  public:  
    typedef std::shared_ptr<PerspectiveProjectile> Pointer;

    /// Create camera
    static Pointer create();

    /// Camera horizontal fov angle
    const math::anglef& fov_x() const;

    /// Set camera horizontal fov angle
    void set_fov_x(const math::anglef& fov_x);

    /// Camera vertical fov angle
    const math::anglef& fov_y() const;

    /// Set camera vertical fov angle
    void set_fov_y(const math::anglef& fov_y);

    /// Camera near Z-plane distance
    float z_near() const;

    /// Set camera near Z-plane distance
    void set_z_near(float z_near);

    /// Camera far Z-plane distance
    float z_far() const;

    /// Set camera far Z-plane distance
    void set_z_far(float z_far);

  protected:
    /// Constructor
    PerspectiveProjectile();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    void recompute_projection_matrix() override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}}
