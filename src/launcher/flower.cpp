#include "flower.h"

#include <math/constants.h>

#include <cmath>
#include <vector>

using namespace engine::media::geometry;

namespace engine {
namespace launcher {

namespace
{

const float PI         = math::constf::pi;
const float TWO_PI     = 2.0f * PI;
const float GOLDEN_DEG = 137.50776405f;           // golden angle (ABOP ch.4 / Vogel)
const float GOLDEN_RAD = GOLDEN_DEG * PI / 180.0f;

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float lerpf(float a, float b, float t)    { return a + (b - a) * t; }

// classic smoothstep
float smoothstepf(float e0, float e1, float x)
{
  float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

math::vec3f mix3(const math::vec3f& a, const math::vec3f& b, float t)
{
  return a + (b - a) * t;
}

// deterministic hash -> float in [0,1)
float hashf(uint32_t x)
{
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return (x & 0x00ffffffU) / float(0x01000000U);
}

// An orthonormal frame (n,b) spanning the plane perpendicular to unit tangent t.
void frame(const math::vec3f& t, math::vec3f& n, math::vec3f& b)
{
  math::vec3f a = (std::fabs(t.y) < 0.95f) ? math::vec3f(0, 1, 0) : math::vec3f(1, 0, 0);
  n = math::normalize(math::cross(a, t));
  b = math::normalize(math::cross(t, n));
}

// Accumulates geometry, then emits one primitive.
struct Builder
{
  std::vector<Vertex>            verts;
  std::vector<Mesh::index_type>  indices;

  uint32_t add(const math::vec3f& p, const math::vec3f& n, const math::vec3f& c)
  {
    Vertex v;
    v.position  = p;
    v.normal    = n;
    v.color     = math::vec4f(c.x, c.y, c.z, 1.0f);
    v.tex_coord = math::vec2f(0.0f, 0.0f);
    verts.push_back(v);
    return (uint32_t) verts.size() - 1;
  }

  void tri(uint32_t a, uint32_t b, uint32_t c)
  {
    indices.push_back((Mesh::index_type) a);
    indices.push_back((Mesh::index_type) b);
    indices.push_back((Mesh::index_type) c);
  }

  // a planar quad (a,b,c,d ccw), both triangles
  void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
  {
    tri(a, b, c);
    tri(a, c, d);
  }
};

// stem path P(t), t in [0,1], for height H with a lateral tip sway `bend*H`.
math::vec3f stem_point(float t, float H, float bend)
{
  return math::vec3f(bend * H * t * t, H * t, 0.0f);
}

math::vec3f stem_tangent(float t, float H, float bend)
{
  return math::normalize(math::vec3f(2.0f * bend * H * t, H, 0.0f));
}

} // namespace

FlowerParams make_flower_params(uint32_t seed)
{
  FlowerParams p;
  p.seed = seed;

  p.petal_count  = 5 + (int) (hashf(seed * 2654435761u + 1u) * 5.0f);   // 5..9
  p.max_height   = lerpf(0.46f, 0.78f, hashf(seed + 7u));
  p.head_radius  = lerpf(0.065f, 0.105f, hashf(seed + 13u));
  p.petal_length = lerpf(0.12f, 0.20f, hashf(seed + 23u));
  p.bend         = lerpf(-0.10f, 0.10f, hashf(seed + 31u));

  // pick a petal hue around warm pinks/reds/violets/yellows
  float h = hashf(seed + 41u);
  math::vec3f a(0.93f, 0.34f, 0.55f);  // pink
  math::vec3f b(0.97f, 0.78f, 0.20f);  // yellow
  math::vec3f c(0.74f, 0.36f, 0.86f);  // violet
  p.petal_color = (h < 0.5f) ? mix3(a, b, h * 2.0f) : mix3(b, c, (h - 0.5f) * 2.0f);
  p.petal_tip   = mix3(p.petal_color, math::vec3f(1.0f, 0.96f, 0.92f), 0.55f);
  return p;
}

void generate_flower(Mesh& out, const FlowerParams& p, float g, const char* material)
{
  out.clear();
  g = clampf(g, 0.0f, 1.0f);

  Builder bld;

  // ----- growth-driven dimensions -----
  const float gh   = smoothstepf(0.0f, 1.0f, g);
  const float H    = lerpf(0.10f, p.max_height, gh);
  const float rb   = p.stem_radius * lerpf(0.55f, 1.0f, gh);
  const float Rh   = p.head_radius * smoothstepf(0.15f, 1.0f, g);
  const float Lp   = p.petal_length * smoothstepf(0.10f, 1.0f, g);
  const float open = smoothstepf(0.18f, 0.85f, g);   // 0 = closed bud, 1 = open whorl

  // ===== 1. STEM (swept, tapered tube) =====
  const int rings = p.stem_rings;
  const int sides = p.stem_sides;
  std::vector<uint32_t> ring_base(rings + 1);

  for (int i = 0; i <= rings; ++i)
  {
    float t = (float) i / (float) rings;
    math::vec3f c  = stem_point(t, H, p.bend);
    math::vec3f tg = stem_tangent(t, H, p.bend);
    math::vec3f n, b;
    frame(tg, n, b);
    float r = rb * (1.0f - 0.55f * t);

    ring_base[i] = bld.verts.size();
    for (int j = 0; j < sides; ++j)
    {
      float a   = TWO_PI * (float) j / (float) sides;
      math::vec3f dir = n * std::cos(a) + b * std::sin(a);
      bld.add(c + dir * r, dir, p.stem_color);
    }
  }
  for (int i = 0; i < rings; ++i)
    for (int j = 0; j < sides; ++j)
    {
      int j2 = (j + 1) % sides;
      bld.quad(ring_base[i] + j, ring_base[i] + j2,
               ring_base[i + 1] + j2, ring_base[i + 1] + j);
    }

  // ===== head frame (at the stem tip) =====
  math::vec3f head_c  = stem_point(1.0f, H, p.bend);
  math::vec3f up      = stem_tangent(1.0f, H, p.bend);  // flower "up" = stem direction
  math::vec3f U, V;
  frame(up, U, V);
  const float dome = Rh * 0.35f;

  // ===== 2. RECEPTACLE (a low domed disk) =====
  if (Rh > 1e-4f)
  {
    const int disk = 24;
    uint32_t apex = bld.add(head_c + up * dome, up, p.center_color);
    uint32_t r0   = bld.verts.size();
    for (int j = 0; j < disk; ++j)
    {
      float a = TWO_PI * (float) j / (float) disk;
      math::vec3f dir = U * std::cos(a) + V * std::sin(a);
      math::vec3f nrm = math::normalize(up * 1.4f + dir);
      bld.add(head_c + dir * Rh, nrm, mix3(p.center_color, p.floret_color, 0.6f));
    }
    for (int j = 0; j < disk; ++j)
      bld.tri(apex, r0 + j, r0 + (j + 1) % disk);
  }

  // ===== 3. FLORETS (golden-angle Vogel spiral) =====
  int florets = (int) (p.floret_max * smoothstepf(0.35f, 1.0f, g));
  if (Rh > 1e-4f && florets > 0)
  {
    float c_scale = Rh / std::sqrt((float) florets);   // outermost floret ~ rim
    float fs      = Rh * 0.11f;                          // floret quad half-size
    for (int n = 0; n < florets; ++n)
    {
      float ang = n * GOLDEN_RAD;
      float rad = c_scale * std::sqrt((float) n);
      math::vec3f dir = U * std::cos(ang) + V * std::sin(ang);
      math::vec3f c   = head_c + dir * rad + up * (dome * (1.0f - (rad / (Rh + 1e-4f))) + fs * 0.5f);
      math::vec3f col = mix3(p.center_color, p.floret_color, clampf(rad / (Rh + 1e-4f), 0.0f, 1.0f));
      uint32_t a = bld.add(c - U * fs - V * fs, up, col);
      uint32_t b = bld.add(c + U * fs - V * fs, up, col);
      uint32_t d = bld.add(c + U * fs + V * fs, up, col);
      uint32_t e = bld.add(c - U * fs + V * fs, up, col);
      bld.quad(a, b, d, e);
    }
  }

  // ===== 4. PETAL WHORL =====
  const int K = p.petal_count;
  const int seg = p.petal_segments;
  if (Lp > 1e-4f)
  {
    for (int k = 0; k < K; ++k)
    {
      float phi = TWO_PI * (float) k / (float) K;
      math::vec3f D = U * std::cos(phi) + V * std::sin(phi);   // radial-out direction
      math::vec3f S = math::normalize(math::cross(up, D));     // petal sideways axis (constant)
      math::vec3f base = head_c + D * (Rh * 0.55f);

      // open angle from vertical: closed bud hugs the head, open whorl folds outward
      float theta = lerpf(0.12f, 1.48f, open);  // radians (~7deg -> ~85deg)

      std::vector<uint32_t> left(seg + 1), right(seg + 1);
      math::vec3f pos = base;
      for (int i = 0; i <= seg; ++i)
      {
        float u = (float) i / (float) seg;
        float a = theta * u;
        math::vec3f dir = up * std::cos(a) + D * std::sin(a);
        if (i > 0) pos = pos + dir * (Lp / (float) seg);

        float w   = p.petal_width * (0.30f + 0.70f * std::sin(PI * clampf(0.08f + 0.86f * u, 0.0f, 1.0f)));
        math::vec3f nrm = math::normalize(math::cross(S, dir));
        math::vec3f col = mix3(mix3(p.center_color, p.petal_color, 0.5f), p.petal_tip, u);
        left[i]  = bld.add(pos + S * (w * 0.5f), nrm, col);
        right[i] = bld.add(pos - S * (w * 0.5f), nrm, col);
      }
      for (int i = 0; i < seg; ++i)
        bld.quad(left[i], right[i], right[i + 1], left[i + 1]);
    }
  }

  if (bld.verts.empty())
    return;

  out.add_primitive(material, PrimitiveType_TriangleList,
                    &bld.verts[0], (Mesh::index_type) bld.verts.size(),
                    &bld.indices[0], (uint32_t) bld.indices.size());
}

}}
