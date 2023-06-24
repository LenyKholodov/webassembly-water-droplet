#pragma once

#include <math/vector.h>
#include <media/geometry.h>

/// Hull builder
class HullBuilder
{
  public:
    /// Constructor
    HullBuilder ();

    /// Destructor
    ~HullBuilder ();

    /// Reserve space for points
    void reserve (size_t points_count);

    /// Reset builder
    void reset ();

    /// Add point to builder
    void add_point (const math::vec3f& position);

    /// Smooth level setter
    void set_smooth_level (unsigned short tesselationLevel, unsigned short refineLevel);

    /// Smooth level getter
    unsigned short smooth_level () const;

    /// Refine level getter
    unsigned short refine_level () const;

    /// Mesh
    const engine::media::geometry::Mesh& mesh() const;

    /// Build hull
    bool build_hull(const char* material_name);

  private:
    HullBuilder (const HullBuilder&); //no implementation
    HullBuilder& operator = (const HullBuilder&); //no implementation

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

typedef std::vector<engine::media::geometry::Vertex> HullVertexArray;
typedef std::vector<engine::media::geometry::Mesh::index_type> HullIndexArray;

/// Hull smoother interface
class IHullSmoother
{
  public:
    /// Destructor
    virtual ~IHullSmoother() {}

    virtual void set_smooth_level(unsigned short tesselLevel, unsigned short refineLevel) { }

    /// Smooth action
    virtual void smooth(const HullVertexArray& in_vertices,       //input mesh vertices for smoothing
                        const HullIndexArray&  in_indices,        //input mesh indices for smoothing
                        HullVertexArray&       out_vertices,      //output storage for mesh vertices after smoothing
                        HullIndexArray&        out_indices) = 0;  //output storage for mesh indices after smooth
};

/// Tesselation smoother create function
IHullSmoother* create_loop_tesselation_smoother(unsigned short level = 3);

/// Dummy smoother only copies data
IHullSmoother* create_dummy_smoother();
