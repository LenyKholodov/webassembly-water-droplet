#pragma once

// Procedural growing flower (Approach A — parametric developmental flower).
//
// Builds a single flower mesh parameterised by a water "growth budget" g in [0,1]:
//   g=0  -> a tiny closed bud on a short stem
//   g=1  -> a full stem, an open whorl of petals, and a golden-angle floret head
//
// References (see docs/plant-growth-references.md):
//   * Vogel / ABOP ch.4  -> the floret head: theta(n)=n*137.5deg, r(n)=c*sqrt(n)
//   * Ijiri et al. 2005  -> the floral-diagram structure: receptacle + petal whorl + florets
//
// The generator only touches media::geometry (vertices/indices/primitives) and math, so it has no
// dependency on the renderer. Colours are written into Vertex.color and shown by the dedicated
// "flower" shader (media/shaders/flower.glsl), which uses the vertex colour as lit albedo.

#include <media/geometry.h>
#include <math/vector.h>

#include <cstdint>

namespace engine {
namespace launcher {

struct FlowerParams
{
  uint32_t seed = 0;             // per-plant RNG seed -> shape/colour variety

  // structure counts (kept modest; uint16 index budget is plenty)
  int   stem_sides    = 7;       // radial segments of the stem tube
  int   stem_rings    = 16;      // rings along the stem
  int   petal_count   = 7;       // petals in the whorl
  int   petal_segments = 6;      // length subdivisions per petal
  int   floret_max    = 80;      // florets at full growth (Vogel disk)

  // dimensions at full growth (metres, local space; flower base at origin, grows +Y)
  float max_height    = 0.62f;   // stem length at g=1
  float stem_radius   = 0.018f;  // stem radius at the base
  float head_radius   = 0.085f;  // flower-head (receptacle) radius at g=1
  float petal_length  = 0.16f;   // petal length at g=1
  float petal_width   = 0.075f;  // petal max width
  float bend          = 0.06f;   // lateral sway of the stem tip

  // colours (linear-ish; the flower shader lights them)
  math::vec3f stem_color   = math::vec3f(0.27f, 0.50f, 0.20f);
  math::vec3f petal_color  = math::vec3f(0.93f, 0.34f, 0.55f);
  math::vec3f petal_tip    = math::vec3f(0.99f, 0.74f, 0.86f);
  math::vec3f center_color = math::vec3f(0.85f, 0.62f, 0.12f);
  math::vec3f floret_color = math::vec3f(0.45f, 0.28f, 0.06f);
};

// Derive a varied-but-deterministic parameter set from a seed (height, petal count, hue, ...).
FlowerParams make_flower_params(uint32_t seed);

// (Re)build the flower geometry for growth g in [0,1] into `out` (cleared first). One primitive,
// material name `material` (expected shader tag "flower").
void generate_flower(engine::media::geometry::Mesh& out, const FlowerParams& params, float g,
                     const char* material = "flower");

}}
