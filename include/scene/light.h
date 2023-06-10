#pragma once

#include <scene/node.h>

#include <math/angle.h>

namespace engine {
namespace scene {

const float DEFAULT_LIGHT_RANGE = 1e9;

/// Light source
class Light : public Node
{
  public: 
    typedef std::shared_ptr<Light> Pointer;
  
    /// Light color
    const math::vec3f& light_color() const;    

    /// Set light color
    void set_light_color(const math::vec3f& color);
    
    /// Light intensity
    float intensity() const;

    /// Set light intensity
    void set_intensity(float value);    

    /// Attenuation (constant, linear, quadratic)
    const math::vec3f& attenuation() const;

    /// Set attenuation
    void set_attenuation(const math::vec3f& multiplier);    

    /// Light range
    float range() const;

    /// Set light range
    void set_range(float range);    

  protected:
    /// Constructor
    Light();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    virtual void invalidate_projection() {}

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Cone light source
class SpotLight : public Light
{
  public:
    typedef std::shared_ptr<SpotLight> Pointer;

    /// Create light source
    static Pointer create();

    /// Light cone angle
    const math::anglef& angle() const;

    /// Set light cone angle
    void set_angle(const math::anglef& angle);

    /// Light cone attenuation exponent
    float exponent() const;

    /// Set light cone attenuation exponent
    void set_exponent(float exponent);

    /// Get projection matrix
    const math::mat4f& projection_matrix() const;

  protected:
    /// Constructor
    SpotLight();

    /// Visit node
    void visit(ISceneVisitor&) override;

  private:
    void invalidate_projection() override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Point light source
class PointLight : public Light
{
  public:
    typedef std::shared_ptr<PointLight> Pointer;

    /// Create light source
    static Pointer create();

  protected:
    /// Visit node
    void visit(ISceneVisitor&) override;
};

}}
