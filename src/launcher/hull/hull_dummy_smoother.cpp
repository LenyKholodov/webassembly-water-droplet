#include "hull.h"

namespace
{

class DummySmoother: public IHullSmoother
{
  public:
    void smooth (const HullVertexArray& inVertices, const HullIndexArray& inIndices, HullVertexArray& outVertices, HullIndexArray& outIndices)
    {
      outVertices = inVertices;
      outIndices  = inIndices;
    }
};

}

/// Dummy smoother only copies data
IHullSmoother* create_dummy_smoother ()
{
  return new DummySmoother;
}
