#pragma once

#include <scene/node.h>

#include <math/angle.h>

namespace engine {
namespace scene {

/// Camera
class Camera : public Node
{
  public:
    typedef std::shared_ptr<Camera> Pointer;

    /// Projection matrix
    const math::mat4f& projection_matrix() const;

  protected:
    /// Constructor
    Camera();

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

/// Perspective camera
class PerspectiveCamera : public Camera
{
  public:  
    typedef std::shared_ptr<PerspectiveCamera> Pointer;

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
    PerspectiveCamera();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    void recompute_projection_matrix() override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}}
