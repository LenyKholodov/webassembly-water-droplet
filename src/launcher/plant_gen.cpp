#include "plant_gen.h"

#include <math/constants.h>
#include <math/utility.h>   // rotate(angle, axis)
#include <math/angle.h>     // radian()

#include <cmath>
#include <vector>

using namespace engine::media::geometry;

namespace engine {
namespace launcher {

namespace
{

const float PI         = math::constf::pi;
const float TWO_PI     = 2.0f * PI;
const float GOLDEN_RAD = 137.50776405f * PI / 180.0f;   // golden angle (ABOP ch.4 phyllotaxis)

// per-depth growth span (how much of g a branch level takes to extend). Kept short enough that the
// cumulative birth + random stagger of the deepest twigs still finishes by g=1.
const float SPAN[5] = { 0.24f, 0.18f, 0.15f, 0.12f, 0.10f };

const int MAX_BRANCHES = 160;
const int MAX_SLOTS    = 600;  // safety cap on raw leaf candidates (then evenly downsampled, below)
const int TARGET_LEAVES = 90;  // final leaf count after even downsampling -> bounded Bullet load

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float lerpf(float a, float b, float t)    { return a + (b - a) * t; }

float smoothstepf(float e0, float e1, float x)
{
  if (e1 <= e0) return x < e0 ? 0.0f : 1.0f;
  float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

math::vec3f mix3(const math::vec3f& a, const math::vec3f& b, float t) { return a + (b - a) * t; }

// deterministic hash -> [0,1); the whole plant's randomness keys off a stable per-node id so changing
// g never reshuffles the structure.
float hashf(uint32_t x)
{
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return (x & 0x00ffffffU) / float(0x01000000U);
}

void frame(const math::vec3f& t, math::vec3f& n, math::vec3f& b)
{
  math::vec3f a = (std::fabs(t.y) < 0.95f) ? math::vec3f(0, 1, 0) : math::vec3f(1, 0, 0);
  n = math::normalize(math::cross(a, t));
  b = math::normalize(math::cross(t, n));
}

math::vec3f rotate_about(const math::vec3f& v, const math::vec3f& axis, float ang)
{
  return math::normalize(math::vec3f(math::rotate(math::radian(ang), axis) * v));
}

struct Builder
{
  std::vector<Vertex>           verts;
  std::vector<Mesh::index_type> indices;
  int branches = 0;

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
  void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { tri(a, b, c); tri(a, c, d); }
};

struct Ctx
{
  const PlantParams*     p;
  float                  g;
  Builder*               bld   = nullptr;   // set on the mesh pass
  std::vector<LeafSlot>* slots = nullptr;   // set on the collect pass
  int                    branch_count = 0;
};

// Build one branch (curved, tapered tube) and recurse into children that sprout from it. On the mesh
// pass it emits tube geometry up to the current growth; on the collect pass (g forced to 1) it records
// leaf slots for the mature plant.
void build_branch(Ctx& cx, const math::vec3f& P0, const math::vec3f& dir, float L, float r0,
                  int depth, float birth, uint32_t id)
{
  if (cx.branch_count >= MAX_BRANCHES) return;

  float span    = SPAN[depth < 5 ? depth : 4] * lerpf(0.65f, 1.45f, hashf(id + 777u)); // per-branch growth speed (diversity)
  float mature  = birth + span; if (mature > 1.0f) mature = 1.0f;  // fully extended by g=1 at the latest
  bool  collect = cx.slots != nullptr;
  float local   = collect ? 1.0f : smoothstepf(birth, mature, cx.g);
  if (local <= 0.0f) return;                          // not yet sprouted (mesh pass)
  cx.branch_count++;

  float depth_f = (float) depth / (float) cx.p->max_depth;
  float curL    = L * local;
  int   sides   = cx.p->branch_sides;
  int   rings   = (depth >= cx.p->max_depth - 1) ? 4 : 6;

  // curved centreline: a seeded lateral bend (stronger on thinner, higher branches) plus a gravity
  // droop that accumulates toward the tip.
  math::vec3f bn, bb;
  frame(dir, bn, bb);
  float bendRoll  = hashf(id + 701u) * TWO_PI;
  math::vec3f bendAxis = bn * std::cos(bendRoll) + bb * std::sin(bendRoll);
  float totalBend = (hashf(id + 711u) - 0.5f) * lerpf(0.5f, 1.6f, depth_f);
  float bendStep  = (totalBend / rings) * local;
  float sagStep   = lerpf(0.0f, 0.10f, depth_f) * local;   // droop per segment

  std::vector<math::vec3f> cpos(rings + 1), cdir(rings + 1);
  math::vec3f pos = P0, d = dir;
  float seglen = curL / rings;
  for (int i = 0; i <= rings; ++i)
  {
    cpos[i] = pos; cdir[i] = d;
    d = rotate_about(d, bendAxis, bendStep);
    d = math::normalize(d + math::vec3f(0, -sagStep, 0));   // gravity sag
    pos = pos + d * seglen;
  }

  // tube rings (mesh pass only)
  if (cx.bld)
  {
    std::vector<uint32_t> ringbase(rings + 1);
    for (int i = 0; i <= rings; ++i)
    {
      float t = (float) i / (float) rings;
      math::vec3f rn, rb;
      frame(cdir[i], rn, rb);
      float r = r0 * lerpf(1.0f, 0.32f, t);
      math::vec3f col = mix3(cx.p->stem_base, cx.p->stem_tip, clampf(depth_f + t * 0.4f, 0.0f, 1.0f));
      ringbase[i] = cx.bld->verts.size();
      for (int j = 0; j < sides; ++j)
      {
        float a = TWO_PI * (float) j / (float) sides;
        math::vec3f dirr = rn * std::cos(a) + rb * std::sin(a);
        cx.bld->add(cpos[i] + dirr * r, dirr, col);
      }
    }
    for (int i = 0; i < rings; ++i)
      for (int j = 0; j < sides; ++j)
      {
        int j2 = (j + 1) % sides;
        cx.bld->quad(ringbase[i] + j, ringbase[i] + j2, ringbase[i + 1] + j2, ringbase[i + 1] + j);
      }
  }

  // leaf slots (collect pass only): along the upper portion of branches beyond the trunk
  if (collect && depth >= 1 && (int) cx.slots->size() < MAX_SLOTS)
  {
    int nleaves = 1 + (int) (hashf(id + 53u) * 2.0f) + depth;
    for (int j = 0; j < nleaves && (int) cx.slots->size() < MAX_SLOTS; ++j)
    {
      float t = (j + 0.7f) / (float) nleaves;
      int   i0 = (int) (t * rings); if (i0 >= rings) i0 = rings - 1;
      math::vec3f at  = cpos[i0];
      math::vec3f bdr = cdir[i0];
      math::vec3f ln, lb;
      frame(bdr, ln, lb);
      float roll = (float) j * GOLDEN_RAD + hashf(id + 61u) * TWO_PI;
      math::vec3f outAxis = ln * std::cos(roll) + lb * std::sin(roll);
      math::vec3f ldir = math::normalize(bdr * 0.4f + outAxis - math::vec3f(0, 0.12f, 0));

      LeafSlot s;
      s.pos     = at;
      s.dir     = ldir;
      s.birth_g = mature;  // spawn once the branch is fully grown, so the leaf sits ON the finished branch
      s.size    = cx.p->target_height * lerpf(0.022f, 0.050f, hashf(id + (uint32_t) j * 17u + 3u)); // ~4x smaller, varied
      s.seed    = id * 131u + (uint32_t) j;
      cx.slots->push_back(s);
    }
  }

  // children sprout from the now-full-length branch, progressively (each at a slightly randomised g)
  if (depth < cx.p->max_depth)
  {
    int nchild = 2 + (hashf(id + 31u) > 0.45f ? 1 : 0) + (hashf(id + 37u) > 0.82f ? 1 : 0);
    for (int c = 0; c < nchild; ++c)
    {
      uint32_t cid = id * 8u + (uint32_t) c + 1u;
      float attach_t = lerpf(0.45f, 1.0f, hashf(cid + 41u));
      int   i0 = (int) (attach_t * rings); if (i0 >= rings) i0 = rings - 1;
      math::vec3f atp  = cpos[i0];
      math::vec3f atd  = cdir[i0];
      math::vec3f tn, tb;
      frame(atd, tn, tb);

      float stagger = lerpf(0.0f, 0.03f, hashf(cid + 43u));
      float child_birth = mature + stagger;               // appears after this branch matures
      float roll  = (float) c * (TWO_PI / nchild) + hashf(cid + 13u) * 1.3f;
      math::vec3f axis = tn * std::cos(roll) + tb * std::sin(roll);
      float ang   = lerpf(0.50f, 1.05f, hashf(cid + 17u));   // ~29..60 deg off the parent
      math::vec3f cdirc = rotate_about(atd, axis, ang);
      cdirc = math::normalize(cdirc + math::vec3f(0, 0.25f, 0));    // upward bias
      float cL = L * lerpf(0.60f, 0.80f, hashf(cid + 23u));
      float cR = r0 * lerpf(0.55f, 0.70f, hashf(cid + 29u));
      build_branch(cx, atp, cdirc, cL, cR, depth + 1, child_birth, cid);
    }
  }
}

math::vec3f trunk_dir(const PlantParams& p)
{
  return math::normalize(math::vec3f(
      (hashf(p.seed + 101u) - 0.5f) * 0.12f, 1.0f, (hashf(p.seed + 103u) - 0.5f) * 0.12f));
}

} // namespace

PlantParams make_plant_params(uint32_t seed)
{
  PlantParams p;
  p.seed = seed;
  p.target_height = lerpf(16.0f, 20.0f, hashf(seed + 3u));      // ~10x the earlier ~1.8 m plant
  p.trunk_radius  = p.target_height * lerpf(0.020f, 0.028f, hashf(seed + 9u));
  p.max_depth     = 4;
  return p;
}

void generate_plant_mesh(Mesh& out, const PlantParams& p, float g, const char* material)
{
  out.clear();
  g = clampf(g, 0.0f, 1.0f);
  if (g <= 0.0f) return;

  Builder bld;
  Ctx cx; cx.p = &p; cx.g = g; cx.bld = &bld;
  build_branch(cx, math::vec3f(0, 0, 0), trunk_dir(p), p.target_height, p.trunk_radius, 0, 0.0f, p.seed | 1u);

  if (bld.verts.empty()) return;

  out.add_primitive(material, PrimitiveType_TriangleList,
                    &bld.verts[0], (Mesh::index_type) bld.verts.size(),
                    &bld.indices[0], (uint32_t) bld.indices.size());
}

void generate_leaf(Mesh& out, uint32_t seed, float length, const char* material)
{
  out.clear();
  if (length <= 1e-4f) return;

    //shape params (seeded -> diversity)
  float halfW    = length * lerpf(0.20f, 0.38f, hashf(seed * 2654435761u + 1u)); // max half-width
  float skew     = lerpf(0.75f, 1.25f, hashf(seed + 11u));   // where the blade is widest
  float tipSharp = lerpf(0.55f, 1.10f, hashf(seed + 23u));   // tip pointiness
  float petL     = length * lerpf(0.16f, 0.30f, hashf(seed + 37u)); // petiole ("leg") length
  float petR     = length * lerpf(0.012f, 0.020f, hashf(seed + 41u));
  float cup      = lerpf(-0.05f, 0.22f, hashf(seed + 53u));  // edge curl (up = cupped)
  float droop    = lerpf(-0.04f, 0.16f, hashf(seed + 71u)) * length; // tip droop
  const int N    = 10;   // blade length segments
  const int PS   = 5;    // petiole sides

  std::vector<Vertex>           verts;
  std::vector<Mesh::index_type> indices;

  auto addv = [&](const math::vec3f& p, const math::vec3f& n, const math::vec2f& uv) -> uint32_t
  {
    Vertex v;
    v.position  = p;
    v.normal    = math::normalize(n);
    v.color     = math::vec4f(1, 1, 1, 1); // leaf shader uses the texture, not vColor
    v.tex_coord = uv;
    verts.push_back(v);
    return (uint32_t) verts.size() - 1;
  };
  auto tri = [&](uint32_t a, uint32_t b, uint32_t c)
  {
    indices.push_back((Mesh::index_type) a);
    indices.push_back((Mesh::index_type) b);
    indices.push_back((Mesh::index_type) c);
  };

    //--- petiole "leg": a thin tube along +X from the origin (branch) to the blade base ---
  uint32_t pbase = verts.size();
  for (int ri = 0; ri < 2; ri++)
  {
    float x = ri == 0 ? 0.0f : petL;
    float r = ri == 0 ? petR : petR * 0.8f;
    for (int s = 0; s < PS; s++)
    {
      float a = TWO_PI * (float) s / (float) PS;
      math::vec3f dir(0.0f, std::cos(a), std::sin(a));     // ring around +X
      addv(math::vec3f(x, dir.y * r, dir.z * r), dir, math::vec2f(0.72f, 0.04f)); // green base of the tex leaf
    }
  }
  for (int s = 0; s < PS; s++)
  {
    int s2 = (s + 1) % PS;
    uint32_t a = pbase + s, b = pbase + s2, c = pbase + PS + s2, d = pbase + PS + s;
    tri(a, b, c); tri(a, c, d);
  }

    //--- blade: midrib centre C + left/right edges, mapped onto the textured leaf (right half of the png) ---
  std::vector<uint32_t> Ci(N + 1), Li(N + 1), Ri(N + 1);
  for (int i = 0; i <= N; i++)
  {
    float t    = (float) i / (float) N;
    float x    = petL + t * length;
    float ymid = -droop * t * t;                                   // tip droops down
    float hw   = halfW * std::pow(std::sin(PI * std::pow(t, skew)), tipSharp);
    if (hw < 0.0f) hw = 0.0f;
    float yedge = ymid + cup * hw;                                 // edges curl up
    float vtex = 0.10f + 0.80f * t;                                // base->tip along the tex leaf
    Ci[i] = addv(math::vec3f(x, ymid, 0.0f),  math::vec3f(0, 1, 0), math::vec2f(0.72f, vtex));
    Li[i] = addv(math::vec3f(x, yedge, +hw),  math::vec3f(0, 1, 0), math::vec2f(0.58f, vtex));
    Ri[i] = addv(math::vec3f(x, yedge, -hw),  math::vec3f(0, 1, 0), math::vec2f(0.86f, vtex));
  }
  for (int i = 0; i < N; i++)
  {
    tri(Ci[i], Li[i], Li[i + 1]); tri(Ci[i], Li[i + 1], Ci[i + 1]); // left half
    tri(Ci[i], Ri[i + 1], Ri[i]); tri(Ci[i], Ci[i + 1], Ri[i + 1]); // right half
  }

  out.add_primitive(material, PrimitiveType_TriangleList,
                    &verts[0], (Mesh::index_type) verts.size(), &indices[0], (uint32_t) indices.size());
}

void collect_leaf_slots(const PlantParams& p, std::vector<LeafSlot>& out)
{
  out.clear();

    //gather ALL leaf candidates across the whole tree (depth-first order)
  std::vector<LeafSlot> all;
  Ctx cx; cx.p = &p; cx.g = 1.0f; cx.slots = &all;
  build_branch(cx, math::vec3f(0, 0, 0), trunk_dir(p), p.target_height, p.trunk_radius, 0, 0.0f, p.seed | 1u);

  if ((int) all.size() <= TARGET_LEAVES)
  {
    out = all;
    return;
  }

    //downsample with a stride so leaves spread EVENLY over every branch (taking a contiguous prefix
    //would clump them all on the first-traversed subtree -- the bug the user saw).
  float stride = (float) all.size() / (float) TARGET_LEAVES;
  for (int i = 0; i < TARGET_LEAVES; i++)
    out.push_back(all[(size_t) (i * stride)]);
}

}}
