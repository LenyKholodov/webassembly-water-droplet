#pragma once

// Procedural branching plant that grows over time.
//
// A single growth budget g in [0,1] (driven by elapsed time) reveals a seeded, stochastic branch
// structure developmentally: the trunk extends first, then branches sprout from it -- progressively
// and at randomised moments -- then their twigs, branching again. Every random decision (branch
// count, angles, lengths, where/when each branch and leaf appears) is derived from a stable per-node
// hash, NOT a running RNG, so re-generating at a higher g yields the SAME plant, just further grown.
//
// Branches are rendered as curved, tapered tubes (this module). LEAVES are NOT geometry here -- the
// generator only reports leaf attachment "slots" (collect_leaf_slots); the host (world.cpp) spawns a
// real leaf-blade physics body at each slot as growth passes its birth, so leaves look like the demo
// leaves and interact with droplets.
//
// References (docs/plant-growth-references.md): recursive/developmental branching in the spirit of
// the space-colonization work (Runions et al.); leaf placement uses ABOP golden-angle phyllotaxis.

#include <media/geometry.h>
#include <math/vector.h>

#include <cstdint>
#include <vector>

namespace engine {
namespace launcher {

struct PlantParams
{
  uint32_t seed = 0;

  float target_height = 18.0f;   // mature height (world units) -- ~10x the earlier plant
  float trunk_radius  = 0.42f;   // trunk radius at the base (scaled with target_height)
  int   max_depth     = 4;       // branch recursion levels
  int   branch_sides  = 6;       // tube radial segments

  // branch colours (lit by the vertex-colour "flower" material)
  math::vec3f stem_base = math::vec3f(0.32f, 0.22f, 0.12f);  // woody brown at the base
  math::vec3f stem_tip  = math::vec3f(0.40f, 0.52f, 0.22f);  // greener at the growing tips
};

// One leaf attachment: where a leaf blade should be placed, which way it points, when it appears
// (birth_g), and how big (size, in plant-local units). pos/dir are in plant-LOCAL space (base at
// origin, +Y up, before the host's node position/scale).
struct LeafSlot
{
  math::vec3f pos;
  math::vec3f dir;
  float       birth_g = 0.0f;
  float       size    = 0.5f;
  uint32_t    seed    = 0;
};

// Derive a varied-but-deterministic plant from a seed.
PlantParams make_plant_params(uint32_t seed);

// (Re)build the branch geometry for growth g in [0,1] into `out` (cleared first).
void generate_plant_mesh(engine::media::geometry::Mesh& out, const PlantParams& p, float g,
                         const char* material = "flower");

// Collect every leaf attachment slot for the mature plant (each carries its own birth_g).
void collect_leaf_slots(const PlantParams& p, std::vector<LeafSlot>& out);

// One branch as a SKELETON bone: its tube geometry (in bone-local space, relative to rest_base, no
// rotation at rest) + the rest endpoints + parent bone index. The host (world.cpp) makes a rigid body
// per bone, joints it to its parent, and re-poses the verts by the body transform each frame so the
// branch mesh follows the physics skeleton (drag/wind). parent = -1 for the trunk (the fixed root).
struct Bone
{
  math::vec3f rest_base;   // branch base / joint point, plant-local
  math::vec3f rest_tip;    // branch tip, plant-local
  float       radius = 0.1f;
  int         parent = -1;
  std::vector<engine::media::geometry::Vertex>           verts;   // positions relative to rest_base
  std::vector<engine::media::geometry::Mesh::index_type> indices; // 0-based within this bone
};

// Collect the mature plant as a bone skeleton (one bone per branch). Order is parent-before-child.
void collect_bones(const PlantParams& p, std::vector<Bone>& out);

// Generate ONE procedural leaf (a petiole "leg" + a textured blade), seeded for shape diversity.
// Built in leaf-local space: the leg base is at the origin (the branch attachment), the leaf extends
// along +X (tip), the blade normal is +Y, width along Z. `length` is the blade length in world units.
// UVs map the leaf in media/textures/leaf_color.png onto the blade (material is "leaf"). No static
// leaf.obj is used.
void generate_leaf(engine::media::geometry::Mesh& out, uint32_t seed, float length,
                   const char* material = "leaf");

// Collision shape for one leaf, MATCHING generate_leaf's shape: a set of convex point-clouds (build
// each as a btConvexHullShape child of a btCompoundShape) forming a concave V-trough so droplets
// collect in the leaf instead of rolling off a single convex hull. Seed/length must match the leaf.
void generate_leaf_collision(uint32_t seed, float length, std::vector<std::vector<math::vec3f>>& out);

}}
