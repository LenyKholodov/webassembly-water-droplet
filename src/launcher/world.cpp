#include "shared.h"
#include "plant_gen.h"

#include <common/log.h>
#include <common/named_dictionary.h>
#include <math/utility.h>
#include <media/sound_player.h>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"


#include <list>
#include <ctime>
#include <random>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace engine::common;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::scene;
using namespace engine;

const char* LEAF_MESH = "media/meshes/leaf.obj";
const char* PLANT_MESH = "media/meshes/fern.obj";
const size_t CLUSTERIZE_STEPS_COUNT = 3;
const float CLUSTERIZE_STEP_FACTOR = 1.2;
const size_t PREFERRED_MAX_DROPLETS_COUNT = 3;
const float DROPLET_PARTICLE_RADIUS = 0.027f;
const float DROPLET_PARTICLE_MASS = 0.002f;
const float DROPLET_RADIUS = DROPLET_PARTICLE_RADIUS * 20.0f;
const bool DROPLET_DEBUG_DRAW = false;
const float DROPLET_PARTICLE_FORCE_DISTANCE = DROPLET_RADIUS;
// Surface tension is modelled as SPH-style PAIRWISE cohesion between neighbouring particles (Akinci et
// al. 2013), NOT a spring to the cluster centroid. Each near pair attracts with a kernel that vanishes
// at contact and at the cohesion radius and peaks in between -> the blob minimises its surface area and
// holds together (necking/merging) without imploding toward a point.
const float DROPLET_SURFACE_TENSION = 1.0f;    // pairwise cohesion accel (gamma); live via window.DROPLET.force
const float DROPLET_VISCOSITY       = 1.0f;    // pairwise relative-velocity damping rate (settles internal jiggle, keeps the fall); live via window.DROPLET.damping
const float DROPLET_COHESION_RADIUS = 0.11f;   // neighbour range h (~4x particle radius); live via window.DROPLET.cohesionRadius
const float DROPLET_PARTICLE_MIN_INTERACTION_RADIUS = DROPLET_PARTICLE_RADIUS * 4.0f;
const float COLLISION_MARGIN = 0.001f;
const float DROPLET_PARTICLE_MIN_FRICTION = 0.35;
const float DROPLET_PARTICLE_MAX_FRICTION = 0.9;
const float DROPLET_MIN_FRICTION_FACTOR = 0.2f;
const float DROPLET_MAX_FRICTION_FACTOR = 2.;
const size_t DROPLET_INITIAL_LEAF = 0;
const float DROPLET_PARTICLE_LINEAR_SLEEPING_THRESHOLD = 1.f;
const float DROPLET_PARTICLE_ANGULAR_SLEEPING_THRESHOLD = 1.f;
const size_t DROPLET_CENTER_APPROXIMATION_STEPS_COUNT = 3;
const size_t DROPLET_GENERATION_INTERVAL = 5 * CLOCKS_PER_SEC;
const size_t MIN_DROPLET_PARTICLES_COUNT = 10;
const float MIN_DROPLET_PARTICLE_HEIGHT = -6.f;
const size_t DROPLET_REMOVE_COUNTER_THRESHOLD = 30;
const float DROPLET_PLANT_GENERATION_HEIGHT = MIN_DROPLET_PARTICLE_HEIGHT + 0.5f;

// Droplet surface: a metaball SDF raymarched in a fragment shader (true concavity/necking/merges,
// exact normals), reflecting the skybox cubemap + screen-space refraction. (The old convex-hull +
// Loop-subdivision surface was removed.)
const size_t MAX_DROPLET_RAYMARCH_PARTICLES = 64;                              // MUST match MAX_DROPLET_PARTICLES in droplet_fluid.glsl
const float  DROPLET_RAYMARCH_PARTICLE_RADIUS = 0.050f; // per-particle metaball sphere radius — the rendered sphere size (independent of the physical radius)
const float  DROPLET_INFLUENCE_RADIUS = 0.075f;         // smooth-min blend k: bigger -> spheres merge into one coherent blob
const float  DROPLET_ISO_THRESHOLD = -0.04f;            // surface iso level: inflate (+) / thin (-)
const float  DROPLET_RAYMARCH_BOX_MARGIN = 1.08f;                              // proxy-box slack; tight so leaves occlude the droplet (box is depth-tested) without much overdraw
// Droplet reflection source: true -> the static skybox cubemap (cheap, consistent, and skips the
// per-droplet dynamic env-map render); false -> a per-droplet cubemap rendered from the cluster centre.
const bool   DROPLET_REFLECT_SKYBOX = true;
static size_t PARALLELS_COUNT = 5, MERIDIANS_COUNT = 5; // per-shell spawn grid; total particles = live particles/droplet
const size_t MAX_PARTICLES_COUNT = 600;                                  // total particle budget (recycled oldest-first when exceeded)
const math::vec3f LEAVES_SCALE(0.1f);
const math::vec3f PLANT_SCALE(0.005f);
const float LEAF_MASS = 1.0f;
const float LEAF_MIN_FRICTION = 0.04f; // low -> water slides easily along the leaf
const float LEAF_MAX_FRICTION = 0.12f;
const math::vec3f STEAM_POSITION(0, 0, 0);
const clock_t DEBUG_DUMP_INTERVAL = 5 * CLOCKS_PER_SEC;
const clock_t PLAY_CONTACT_SOUND_IF_NO_CONTACTS_DURING = CLOCKS_PER_SEC / 2;
const size_t PLAY_CONTACT_SOUND_COLLISIONS_COUNT = 5;

const float GROUND_SIZE = 50.0f;
const float GROUND_OFFSET = -7.f;

const char* DROPLET_HULL_MATERIAL = "droplet";
const char* DROPLET_FLUID_MATERIAL = "droplet_fluid";
const int COLLISION_GROUP_DROPLET = 1;
const int COLLISION_GROUP_GROUND = 1 << 1;
const int COLLISION_GROUP_LEAF = 1 << 2;
const int COLLISION_MASK_DROPLET = COLLISION_GROUP_GROUND | COLLISION_GROUP_LEAF | COLLISION_GROUP_DROPLET;
const int COLLISION_MASK_GROUND = COLLISION_GROUP_DROPLET;
const int COLLISION_MASK_LEAF = COLLISION_GROUP_DROPLET;
const float DRAG_FORCE_MULTIPLIER = 10.f;
const float DRAG_MAX_FORCE = 2.f;

const math::vec3f LEAF_LIGHT_OFFSET(0, 0.5, 0);
const float LIGHTS_MIN_INTENSITY = 0.10f;
const float LIGHTS_MAX_INTENSITY = 0.18;
const float LIGHTS_MIN_RANGE = 2.5;
const float LIGHTS_MAX_RANGE = 3.5;
const math::vec3f LIGHTS_ATTENUATION(1, 1, 0.5);

const float PLANT_GENERATION_HEIGHT = GROUND_OFFSET;
const float PLANT_GENERATION_RADIUS = 10.f;
const float PLANT_SAFE_ZONE_RADIUS = 1.5f;
const float PLANT_LIGHT_ZONE_SIZE = 15.f;
const float PLANT_LIGHT_RANGE_FACTOR = PLANT_LIGHT_ZONE_SIZE / LIGHTS_MAX_RANGE;
const float PLANT_LIGHT_HEIGHT = GROUND_OFFSET + 2.f;
const float PLANT_MAX_SCALE = 3.0f;
const float PLANT_SCALE_STEP = 2.0;
const float PLANT_GROW_CHANCE = 0.25f;
const size_t PLANT_FALLEN_DROPLET_PARTICLES_COUNT_THRESHOLD = 25;

// Procedural branching-plant growth (see docs/plant-growth-references.md). A plant sprouts when
// water accumulates, then grows its branch structure over PLANT_GROW_SECONDS up to mature height.
const float  PLANT_GROW_SECONDS = 60.0f;  // time from sprout to full structure (the "1 minute")
const float  PLANT_REBUILD_DG   = 0.006f; // re-mesh when growth advances this much (smooth, cheap)
const size_t PLANT_MAX_COUNT    = 36;     // cap concurrent plants (vertex/perf budget)
const float  LEAF_GROW_START       = 0.05f; // leaf scale at spawn (then ramps to 1)
const float  LEAF_GROW_MIN_SECONDS = 1.5f;  // a leaf unfurls over this..max seconds (random per leaf)
const float  LEAF_GROW_MAX_SECONDS = 4.5f;
const float  DROPLET_SPAWN_ABOVE_LEAF = 0.4f; // spawn the droplet a little above the chosen top leaf
// branch-skeleton spring joints (tune live): stiffness scales with branch thickness
const float  JOINT_STIFFNESS_BASE = 1000.0f; // * (mass^2 + eps) per joint; live via window.WIND.stiffness
const float  JOINT_DAMPING        = 0.7f;    // 0..1 spring damping; live via window.WIND.damping
const float  JOINT_ANGLE_LIMIT    = 0.9f;    // max bend per joint (radians)
const float  WIND_ACCEL           = 10.0f;   // wind acceleration on the branch bones; live via window.WIND.accel

const float WATER_SURFACE_SIZE = GROUND_SIZE * 5.0f; // a sea reaching the horizon (matches the platform extent)
const float WATER_LEVEL = GROUND_OFFSET + 1.0f;      // water sits ABOVE the platform, so the platform is submerged under it
const float WATER_SURFACE_OFFSET = WATER_LEVEL;
const size_t WATER_SURFACE_GRID_SIZE = 160;          // a bit denser so ripples stay smooth on the larger plane
const float WATER_DEPTH = WATER_LEVEL - GROUND_OFFSET;  // depth of the pool over the platform (used to clamp wave troughs)
const float WATER_HEIGHT_SCALE = 0.5f;               // small vertical displacement -> gentle ripples, no platform clipping
const float WATER_NORMAL_STEEPNESS = 14.0f;          // how strongly droplet ripples perturb the normal (lowered -> lighter, softer chop)
const float WATER_SPLASH_STRENGTH = 0.001f;          // per-particle droplet impact (a droplet = many particles, so they accumulate)
const float WATER_SPLASH_MAX_DIP = 0.015f;            // cap the per-droplet dip -> a small, clearly-visible impact ring (well below the ~0.08 swell)
const int   WATER_SPLASH_RADIUS = 1;                 // radius (in grid cells, ~3 world units each) of the impact ring -> a tiny, pin-point footprint (1 cell is the grid floor)
const float WATER_AMBIENT_SPLASH_CHANCE = 0.05f;     // frequent but...
const float WATER_AMBIENT_SPLASH_STRENGTH = 0.004f;  // ...tiny random ripples, like wind on water
const float WATER_WAVE_SPEED = 0.25f;                // <1 slows wave propagation -> calm, relaxing water

  //a permanent, always-animating swell layered under the droplet ripples so the surface always reads as living water
const float WATER_SWELL_AMP1   = 0.05f;              // height of the primary swell (world units)
const float WATER_SWELL_LEN1   = 15.0f;              // wavelength of the primary swell (world units)
const float WATER_SWELL_SPEED1 = 1.1f;              // angular speed of the primary swell
const float WATER_SWELL_AMP2   = 0.028f;             // secondary cross-swell, breaks up the regularity
const float WATER_SWELL_LEN2   = 9.0f;
const float WATER_SWELL_SPEED2 = 1.7f;
const float WATER_SWELL_STEEPNESS = 2.2f;            // how strongly the swell tilts the normal (drives the gentle moving reflection)
const float WATER_SWELL_TIME_STEP = 0.016f;          // swell clock advance per update (~one frame)
const char* WATER_SURFACE_MATERIAL_NAME = "water";

const char* SKY_MATERIAL = "sky";
const float SKY_RADIUS = 100.0f;
const char* SKY_TEXTURE_PATH = "media/textures/sky.jpg";

// Fireflies: small green additive glow spheres that rise around the tree, pulse, and wander,
// each carrying a local green point light. Reference: real fireflies drift slowly upward with
// a gentle wandering path and a soft on/off glow.
const size_t FIREFLY_COUNT = 8;
const float FIREFLY_RADIUS = 0.07f;
const float FIREFLY_SPAWN_RADIUS = 3.2f;             // scattered around the tree
const float FIREFLY_BOTTOM = GROUND_OFFSET + 0.5f;   // rise from near the water...
const float FIREFLY_TOP = 2.5f;                      // ...up past the top of the tree
const float FIREFLY_DRIFT = 1.1f;                    // horizontal wander amplitude
const math::vec3f FIREFLY_COLOR(0.35f, 1.0f, 0.45f); // green
const float FIREFLY_LIGHT_INTENSITY = 0.32f;         // soft local green lighting on leaves/droplets
const float FIREFLY_LIGHT_RANGE = 2.8f;

//todo: remove motion states from rigid bodies

namespace
{

float frand()
{
  return rand() / float(RAND_MAX);
}

float crand(float min=-1.0f, float max=1.0f)
{
  return frand() * (max - min) + min;
}

struct RigidBodyWorldCommonData
{
  size_t leaves_collisions_count = 0;
  clock_t last_leaf_contact_sound_played_time = 0;
};

struct RigidBodyInfo
{
  int collision_group;                //collision group of this object
  clock_t prev_droplet_contact_time;  //time when previous contact with droplet occured
  const clock_t& last_frame_time;     //last frame time
  RigidBodyWorldCommonData* world_data; //common data for all rigid bodies in the world

  RigidBodyInfo(int in_collision_group, const clock_t& in_last_frame_time, RigidBodyWorldCommonData& world_data)
    : collision_group(in_collision_group)
    , prev_droplet_contact_time(0)
    , last_frame_time(in_last_frame_time)
    , world_data(&world_data)
    {}
};

struct DropletParticle
{
  bool fallen = false;
};

struct PhysBodySync
{
  std::shared_ptr<btDiscreteDynamicsWorld> dynamics_world;
  std::shared_ptr<btCollisionShape> shape;
  std::shared_ptr<btDefaultMotionState> motion_state;
  std::shared_ptr<btRigidBody> body;
  scene::Mesh::Pointer mesh;
  std::shared_ptr<DropletParticle> droplet_particle;

  PhysBodySync(
    const std::shared_ptr<btCollisionShape>& shape,
    float mass,
    const math::vec3f& local_intertia,
    const math::vec3f& position,
    const math::quatf& rotation,
    const scene::Mesh::Pointer& mesh,
    int collision_group,
    int collision_mask,
    const std::shared_ptr<btDiscreteDynamicsWorld>& dynamics_world)
    : dynamics_world(dynamics_world) // was never stored -> the destructor dereferenced a null world (latent until bodies were actually destroyed)
    , mesh(mesh)
    , shape(shape)
  {
    btTransform start_transform;
    start_transform.setIdentity();
    start_transform.setOrigin(btVector3(position[0], position[1], position[2]));
    start_transform.setRotation(btQuaternion(rotation[0], rotation[1], rotation[2], rotation[3]));

    motion_state = std::make_shared<btDefaultMotionState>(start_transform);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state.get(), shape.get(), btVector3(local_intertia[0], local_intertia[1], local_intertia[2]));
    body = std::make_shared<btRigidBody>(rb_info);

    if (collision_group == COLLISION_GROUP_DROPLET)
    {
      //we need callbacks for droplet collisions
      body->setCollisionFlags (body->getCollisionFlags () | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    }

    dynamics_world->addRigidBody(body.get(), collision_group, collision_mask);
  }

  ~PhysBodySync()
  {
    dynamics_world->removeRigidBody(body.get());
  }
};

struct Leaf
{
  std::shared_ptr<RigidBodyInfo> rigid_body_info;
  std::shared_ptr<PhysBodySync> phys_body;
  std::shared_ptr<btRigidBody> static_bind_body;
  std::shared_ptr<btTypedConstraint> constraint;
  btTransform target_transform;
  math::vec3f initial_center;
  math::vec3f local_center; // mesh centroid in body-local space, to spawn droplets at the leaf's CURRENT pose
  scene::PointLight::Pointer point_light;
  // a generated leaf grows in: scale ramps 0->1 over grow_duration s (random per leaf for diversity)
  float full_scale = 1.0f;     // mature render/hull scale (the blade's world-length scale)
  float grow_age = 0.0f;       // seconds since the leaf sprouted
  float grow_duration = 0.0f;  // 0 = not a growing generated leaf (e.g. ground), >0 = ramp length
  std::vector<std::shared_ptr<btCollisionShape>> hull_children; // keep compound children alive
  bool on_skeleton = false;             // pinned to a branch bone (follows the swaying branch)
  btRigidBody* skeleton_bone = nullptr; // the branch bone this leaf rides
  btTransform  rest_local;              // leaf's rest pose in that bone's local frame (spring target tracks it)

  Leaf(const clock_t& last_frame_time, RigidBodyWorldCommonData& world_data)
    : rigid_body_info(new RigidBodyInfo(COLLISION_GROUP_LEAF, last_frame_time, world_data))
    {}
};

// One branch as a physics-skeleton bone: a rigid body whose transform poses the branch's tube each
// frame (the branch mesh is rebuilt by transforming each bone's local verts by its body transform).
struct BoneBody
{
  std::shared_ptr<btCollisionShape>     shape;
  std::shared_ptr<btDefaultMotionState> motion;
  std::shared_ptr<btRigidBody>          body;
  std::shared_ptr<btTypedConstraint>    joint; // 6-DOF spring to the parent bone (null for the root)
  int   parent = -1;
  float radius_world = 0.1f; // branch radius in world units
  float mass = 0.0f;         // body mass; joint stiffness scales with mass^2 (structural joints >> twig joints)
  std::vector<media::geometry::Vertex>           verts;   // bone-local, scaled to world units
  std::vector<media::geometry::Mesh::index_type> indices;
};

struct Plant
{
  scene::Mesh::Pointer mesh;
  scene::PointLight::Pointer point_light;
  float scale = 1.0f;
  // procedural branching-plant state: grows from a sprout to full structure over PLANT_GROW_SECONDS.
  launcher::PlantParams params;
  math::vec3f base_position;   // world position of the plant base
  float age = 0.0f;            // seconds since it sprouted
  float growth = 0.0f;         // g in [0,1] = age / PLANT_GROW_SECONDS
  float built_growth = -1.0f;  // g the current mesh was built at (rebuild throttle)
  std::vector<launcher::LeafSlot> slots; // leaf attachment slots (local space), each with a birth_g
  std::vector<char> slot_spawned;        // whether a physics leaf has been spawned for each slot
  // physics skeleton (built once the plant is fully grown; the branch mesh then follows it)
  bool skeletonized = false;
  std::vector<BoneBody> bones;
};

struct PlantLight
{
  scene::PointLight::Pointer point_light;
};

// Live-tunable droplet knobs. Defaults are the compile-time constants; on the web they are overridden
// each frame from window.DROPLET.* (the in-page sliders, see dist/index.html) so the look can be tuned
// without a rebuild. Bake the final values back into the constants above when satisfied.
struct LiveTuning
{
  float metaball_radius = DROPLET_RAYMARCH_PARTICLE_RADIUS;
  float influence       = DROPLET_INFLUENCE_RADIUS;
  float iso             = DROPLET_ISO_THRESHOLD;
  float force           = DROPLET_SURFACE_TENSION; // pairwise cohesion strength (gamma)
  float damping         = DROPLET_VISCOSITY;       // pairwise viscosity
  float cohesion_radius = DROPLET_COHESION_RADIUS; // neighbour range h
  int   particles_per_droplet = 20; // physics particles spawned per droplet (~1.5x fewer)
  float physical_radius = DROPLET_PARTICLE_RADIUS;                                       // Bullet collision sphere radius (+ drives clustering/cohesion radii)
  // branch-skeleton knobs (window.WIND.* sliders)
  float wind_accel      = WIND_ACCEL;
  float joint_stiffness = JOINT_STIFFNESS_BASE;
  float joint_damping   = JOINT_DAMPING;
};

struct Droplet
{
  math::vec3f center;
  std::list<math::vec3f> prev_centers;
  std::vector<math::vec3f> points;
  std::vector<std::shared_ptr<PhysBodySync>> bodies;
  scene::Mesh::Pointer hull_mesh; // the droplet's proxy-box render node (name kept for minimal churn)
  scene::PointLight::Pointer point_light;
  size_t remove_counter = 0;
};

struct Firefly
{
  scene::Mesh::Pointer body;            // small green additive-glow sphere
  scene::PointLight::Pointer light;     // local green light it carries
  Material material;                    // per-firefly material (carries the "glowColor" uniform)
  math::vec3f base;                     // x,z centre of its wandering path
  float time_offset = 0;                // de-syncs the rise cycle between fireflies
  float lifetime = 12;                  // seconds for one bottom->top rise
  float pulse_speed = 2, pulse_phase = 0;
  float drift_speed_x = 0.5f, drift_speed_z = 0.5f;
  float drift_phase_x = 0, drift_phase_z = 0;
};

void find_nearest_point(
  media::geometry::Mesh& mesh,
  const media::geometry::Primitive& primitive1,
  const media::geometry::Primitive& primitive2,
  math::vec3f& out_point1,
  float& nearest_distance)
{
  const media::geometry::Mesh::index_type* index1 = mesh.indices_data() + primitive1.first * 3;
  const media::geometry::Vertex* verts1 = mesh.vertices_data() + primitive1.base_vertex;

  for (size_t i=0, ind_count1=primitive1.count * 3; i<ind_count1; ++i, ++index1)
  {
    const media::geometry::Mesh::index_type* index2 = mesh.indices_data() + primitive2.first * 3;
    const media::geometry::Vertex* verts2 = mesh.vertices_data() + primitive2.base_vertex;
    const math::vec3f& p1 = verts1[*index1].position;

    for (size_t j=0, ind_count2=primitive2.count * 3; j<ind_count2; ++j, ++index2)
    {
      const math::vec3f& p2 = verts2[*index2].position;
      float distance = math::qlen(p1 - p2);
      if (distance < nearest_distance)
      {
        nearest_distance = distance;
        out_point1 = p1;
      }
    }
  }
}

bool contact_added_callback (btManifoldPoint& contact_point,
                             const btCollisionObjectWrapper* object0,
                             int part_id0,
                             int index0,
                             const btCollisionObjectWrapper* object1,
                             int part_id1,
                             int index1)
{
  //We use only btRigidBody for collision objects, so safe to use static_cast here
  RigidBodyInfo *body0_info = (RigidBodyInfo*)object0->m_collisionObject->getUserPointer (),
                *body1_info = (RigidBodyInfo*)object1->m_collisionObject->getUserPointer ();

  //a body without RigidBodyInfo (e.g. a pure constraint anchor) must never be dereferenced
  if (!body0_info || !body1_info)
    return false;

  if (body0_info->collision_group != COLLISION_GROUP_LEAF && body1_info->collision_group != COLLISION_GROUP_LEAF)
  {
    //it is not collision with leaf, ignore
    return false;
  }

  RigidBodyInfo *not_droplet_body_info = body0_info->collision_group == COLLISION_GROUP_DROPLET ? body1_info : body0_info; 
  
  if (not_droplet_body_info->last_frame_time - not_droplet_body_info->prev_droplet_contact_time > PLAY_CONTACT_SOUND_IF_NO_CONTACTS_DURING)
  {
    if (body0_info->world_data) body0_info->world_data->leaves_collisions_count++;
    if (body1_info->world_data) body1_info->world_data->leaves_collisions_count++;

    //engine_log_debug("leaf collisions %d", body0_info->world_data->leaves_collisions_count);

//    engine_log_info("New contact added with group %d at position %f %f %f", not_droplet_body_info->collision_group, contact_point.getPositionWorldOnA().getX(), contact_point.getPositionWorldOnA().getY(), contact_point.getPositionWorldOnA().getZ());
    //media::sound::SoundPlayer::play_sound(media::sound::SoundId::droplet_leaf);
  }

  not_droplet_body_info->prev_droplet_contact_time = not_droplet_body_info->last_frame_time;

  return false;
}

struct PairHasher
{
  size_t operator()(const std::pair<int, int>& v) const { return size_t(v.first * v.second); }
};

struct Field
{
  float U[WATER_SURFACE_GRID_SIZE][WATER_SURFACE_GRID_SIZE];

  Field()
  {
    memset(U, 0, sizeof(U));
  }  
};

//see http://www.gamedev.ru/code/articles/?id=4205 for details
struct WaterSurface
{
  Field A;
  Field B;
  Field* p;
  Field* n;
  media::geometry::Mesh mesh;
  scene::Mesh::Pointer mesh_node;
  float swell_time = 0.0f; // clock for the permanent procedural swell

  // Permanent swell height at world (wx,wz): two slow crossing traveling waves -> a gentle, never-still surface
  static float swell_height(float wx, float wz, float t)
  {
    const float k1 = 6.2831853f / WATER_SWELL_LEN1;
    const float k2 = 6.2831853f / WATER_SWELL_LEN2;
    return WATER_SWELL_AMP1 * sinf(wx * k1 + t * WATER_SWELL_SPEED1)
         + WATER_SWELL_AMP2 * sinf((wx * 0.7f + wz * 0.7f) * k2 + t * WATER_SWELL_SPEED2);
  }

  WaterSurface()
  {
    p = &A;
    n = &B;

    const size_t INDICES_GRID_SIZE = WATER_SURFACE_GRID_SIZE - 1;

    mesh.vertices_resize(WATER_SURFACE_GRID_SIZE * WATER_SURFACE_GRID_SIZE);
    mesh.indices_resize(INDICES_GRID_SIZE * INDICES_GRID_SIZE * 6);

    Vertex* v = mesh.vertices_data();

    for (size_t i=0; i<WATER_SURFACE_GRID_SIZE; i++)
    {
      for (size_t j=0; j<WATER_SURFACE_GRID_SIZE; j++, v++)
      {
          v->position  = math::vec3f(WATER_SURFACE_SIZE * (1.0f - 2.0f * i / float(WATER_SURFACE_GRID_SIZE)), 0, WATER_SURFACE_SIZE * (1.0f - 2.0f * j / float(WATER_SURFACE_GRID_SIZE)));
          v->normal    = math::vec3f(0, 1, 0);
        v->color     = math::vec4f(1.0f);
        v->tex_coord = math::vec2f(j / float(WATER_SURFACE_GRID_SIZE), i / float(WATER_SURFACE_GRID_SIZE));
      }
    }

    media::geometry::Mesh::index_type* ind = mesh.indices_data();

    for (size_t i=0; i<INDICES_GRID_SIZE; i++)
    {
      int row_vertex_offset = i * WATER_SURFACE_GRID_SIZE;

      for (size_t j=0; j<INDICES_GRID_SIZE; j++, ind += 6)
      {
        ind[0] = row_vertex_offset + j;
        ind[1] = row_vertex_offset + j + 1;
        ind[2] = row_vertex_offset + j + WATER_SURFACE_GRID_SIZE;
        ind[3] = row_vertex_offset + j + 1;
        ind[4] = row_vertex_offset + j + WATER_SURFACE_GRID_SIZE + 1;
        ind[5] = row_vertex_offset + j + WATER_SURFACE_GRID_SIZE;
      }
    }

    mesh.add_primitive(WATER_SURFACE_MATERIAL_NAME, media::geometry::PrimitiveType_TriangleList, 0, mesh.indices_count() / 3, 0);

    mesh_node = scene::Mesh::create();

    mesh_node->set_mesh(mesh);
    mesh_node->set_planar_reflection_required(true); // flat mirror: planar reflection + refraction render targets
    mesh_node->set_position(math::vec3f(0, WATER_SURFACE_OFFSET, 0));
    mesh_node->set_scale(math::vec3f(1.0f)); // world scale is baked into the vertices, so the node stays uniform -> normals transform correctly
  }

  // Inject a ripple where a droplet hits the water. world_x/world_z are mapped back to the grid cell.
  void splash(float world_x, float world_z, float strength)
  {
    int ci = int((1.0f - world_x / WATER_SURFACE_SIZE) * 0.5f * float(WATER_SURFACE_GRID_SIZE) + 0.5f);
    int cj = int((1.0f - world_z / WATER_SURFACE_SIZE) * 0.5f * float(WATER_SURFACE_GRID_SIZE) + 0.5f);

    const int R = WATER_SPLASH_RADIUS;
    for (int di=-R; di<=R; di++)
      for (int dj=-R; dj<=R; dj++)
      {
        int i = ci + di, j = cj + dj;
        if (i < 1 || i >= int(WATER_SURFACE_GRID_SIZE) - 1 || j < 1 || j >= int(WATER_SURFACE_GRID_SIZE) - 1)
          continue;
        float falloff = 1.0f - float(di*di + dj*dj) / float(R*R + 1); // smooth, peak = strength at the center
        if (falloff < 0.0f) continue;
        float& u = n->U[i][j];
        u -= falloff * strength;                              // a dip seeds an outward-propagating ring
        if (u < -WATER_SPLASH_MAX_DIP) u = -WATER_SPLASH_MAX_DIP; // ...but overlapping impacts of one droplet's particles can't dig a deep crater
      }
  }

  void update()
  {
      //integrate the wave equation; ripples come from droplet impacts (splash()) plus an occasional faint random one

    if (frand() < WATER_AMBIENT_SPLASH_CHANCE)
      splash(crand() * WATER_SURFACE_SIZE * 0.85f, crand() * WATER_SURFACE_SIZE * 0.85f, WATER_AMBIENT_SPLASH_STRENGTH);

    swell_time += WATER_SWELL_TIME_STEP; // advance the permanent swell

    const float MIN_HEIGHT = -(WATER_DEPTH - 0.15f); // keep wave troughs just above the submerged platform
    const float CELL = 2.0f * WATER_SURFACE_SIZE / float(WATER_SURFACE_GRID_SIZE); // world distance between adjacent grid cells

    Vertex* verts = mesh.vertices_data();

    for (size_t i=1; i<WATER_SURFACE_GRID_SIZE-1; i++)
    {
      for (size_t j=1; j<WATER_SURFACE_GRID_SIZE-1; j++)
      {
        //grid cell (i,j) lives at vertex i*N+j (matching the constructor's row-major layout)
        Vertex* v = &verts[i * WATER_SURFACE_GRID_SIZE + j];

        //permanent swell: sample height + neighbours (x decreases as i grows, z decreases as j grows -> matches the splash differences below)
        float wx = v->position.x, wz = v->position.z;
        float S   = swell_height(wx, wz, swell_time);
        float Sim = swell_height(wx + CELL, wz, swell_time); // i-1
        float Sip = swell_height(wx - CELL, wz, swell_time); // i+1
        float Sjm = swell_height(wx, wz + CELL, swell_time); // j-1
        float Sjp = swell_height(wx, wz - CELL, swell_time); // j+1

        float h = n->U[i][j] * WATER_HEIGHT_SCALE + S;
        v->position.y = h < MIN_HEIGHT ? MIN_HEIGHT : h;
        v->normal.x   = (n->U[i-1][j]-n->U[i+1][j]) * WATER_NORMAL_STEEPNESS + (Sim - Sip) * WATER_SWELL_STEEPNESS;
        v->normal.y   = 1.0f;
        v->normal.z   = (n->U[i][j-1]-n->U[i][j+1]) * WATER_NORMAL_STEEPNESS + (Sjm - Sjp) * WATER_SWELL_STEEPNESS;
        // no CPU normalize: the water shader normalizes worldNormal, and the node scale is uniform, so
        // direction is preserved -> save ~25k sqrt/frame

        constexpr float VIS = 0.110f; // higher viscosity -> droplet ripples attenuate (fade) faster

        float laplas=(n->U[i-1][j]+
                    n->U[i+1][j]+
                n->U[i][j+1]+
                n->U[i][j-1])*0.25f-n->U[i][j];

        p->U[i][j] = ((2.0f-VIS) * n->U[i][j] - p->U[i][j] * (1.0f-VIS) + laplas * WATER_WAVE_SPEED);
      }
    }

      //swap fields

    std::swap(p, n);

      //invalidate mesh

    mesh.touch();
  }
};

}

struct World::Impl: RigidBodyWorldCommonData
{
  media::geometry::Model leaf_model; // still loaded for its leaf_color.png texture; geometry no longer used for leaves
  media::geometry::Model plant_model;
  scene::Node::Pointer scene_root;
  scene::Camera::Pointer camera;
  std::shared_ptr<btDefaultCollisionConfiguration> collision_configuration;
  std::shared_ptr<btCollisionDispatcher> dispatcher;
  std::shared_ptr<btBroadphaseInterface> broadphase;
  std::shared_ptr<btSequentialImpulseConstraintSolver> solver;
  std::shared_ptr<btDiscreteDynamicsWorld> dynamics_world;
  std::shared_ptr<btCollisionShape> ground_shape;
  std::shared_ptr<btCollisionShape> droplet_particle_shape;
  std::shared_ptr<btCollisionShape> static_bind_shape;
  std::vector<std::shared_ptr<PhysBodySync>> phys_bodies;
  media::geometry::Mesh droplet_debug_particle_mesh;
  math::vec3f droplet_particle_local_intertia;
  common::NamedDictionary<std::shared_ptr<btCollisionShape>> convex_shapes;
  std::vector<Leaf> leaves;
  std::vector<std::shared_ptr<PhysBodySync>> droplet_particles;
  std::vector<std::shared_ptr<Droplet>> droplets;
  std::vector<btVector3> st_pos, st_vel, st_acc; // reused scratch for pairwise surface-tension (no per-frame alloc)
  Material droplet_material;
  Material droplet_fluid_material;
  Material sky_material;
  Material water_material;
  Material flower_material; // procedural flowers/branches (vertex-colour lit; tag "flower")
  Material leaf_render_material; // generated leaves, textured with the real leaf_color.png (tag ""/forward_lighting)
  std::vector<std::shared_ptr<Plant>> plants;
  float wind_time = 0.0f; // accumulates dt; drives the wind gusts on the branch skeleton
  btRigidBody* grabbed_object;
  btVector3 grabbed_object_pos_world;
  btVector3 grabbed_object_pos_local;
  clock_t last_frame_time = 0;
  clock_t last_droplet_generated_time = 0;
  clock_t last_debug_dump_time = 0;
  RigidBodyInfo droplet_rigid_body_info;
  RigidBodyInfo ground_rigid_body_info;
  std::unordered_map<std::pair<int, int>, std::shared_ptr<PlantLight>, PairHasher> plant_lights;
  size_t fallen_droplet_particles_count = 0;
  WaterSurface water_surface;
  scene::Mesh::Pointer sky;
  std::vector<Firefly> fireflies;
  LiveTuning live; // droplet knobs, refreshed from the in-page sliders each frame (web)

  Impl(scene::Node::Pointer scene_root, SceneRenderer& scene_renderer, const scene::Camera::Pointer& camera)
    : leaf_model(media::geometry::MeshFactory::load_obj_model(LEAF_MESH))
    , plant_model(media::geometry::MeshFactory::load_obj_model(PLANT_MESH))
    , scene_root(scene_root)
    , camera(camera)
    , collision_configuration(new btDefaultCollisionConfiguration())
    , dispatcher(new btCollisionDispatcher(collision_configuration.get()))
    , broadphase(new btDbvtBroadphase())
    , solver(new btSequentialImpulseConstraintSolver())
    , dynamics_world(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collision_configuration.get()))
    , droplet_debug_particle_mesh(media::geometry::MeshFactory::create_sphere("mtl1", DROPLET_PARTICLE_RADIUS))
    , grabbed_object(0)
    , droplet_rigid_body_info(COLLISION_GROUP_DROPLET, last_frame_time, *this)
    , ground_rigid_body_info(COLLISION_GROUP_GROUND, last_frame_time, *this)
  {
      //load materials

    Device render_device = scene_renderer.device();
    MaterialList materials = scene_renderer.materials();

    load_materials(leaf_model, materials, render_device);
    load_materials(plant_model, materials, render_device);

      //configure materials

    droplet_material.set_shader_tags("fresnel");
    droplet_material.set_textures(materials.find("mtl1")->textures());
    droplet_material.set_properties(materials.find("mtl1")->properties());

    // metaball-raymarch droplet material. Refraction is the screen-space scene texture (bound per draw);
    // reflection is the skybox cubemap (added below) or, in dynamic mode, the per-droplet env-map (per draw).
    // The shader samples neither diffuseTexture nor material properties, so none are needed here.
    droplet_fluid_material.set_shader_tags("droplet_fluid");

    Texture sky_texture = render_device.create_texture_cubemap(SKY_TEXTURE_PATH);

    sky_texture.set_min_filter(TextureFilter_Linear);

    // default: droplets reflect the static skybox cubemap (see DROPLET_REFLECT_SKYBOX).
    // textures() is a const ref; copy the handle (shares impl) then insert, like the sky/water materials.
    if (DROPLET_REFLECT_SKYBOX)
    {
      TextureList droplet_fluid_textures = droplet_fluid_material.textures();
      droplet_fluid_textures.insert("environmentMap", sky_texture);
    }

    TextureList sky_textures = sky_material.textures();
    sky_textures.insert("diffuseTexture", sky_texture);

    sky_material.set_shader_tags("sky");

    // the water surface reflects the sky cubemap directly (no scene env-map -> no parallax smearing on the flat plane)
    water_material.set_shader_tags("water");
    TextureList water_textures = water_material.textures();
    water_textures.insert("skyTexture", sky_texture);

    flower_material.set_shader_tags("flower"); // generator paints per-vertex; shader lights it (no texture)

    // Textured leaf material: the real leaf_color.png as albedo (lit by forward_lighting + moonlight).
    // Loaded from the correct MEMFS path -- leaf.mtl's "../textures/..." path does NOT resolve, which is
    // why the leaf.obj material rendered black; this binds the texture explicitly.
    {
      Texture leaf_diffuse = render_device.create_texture2d("media/textures/leaf_color.png");
      Texture leaf_normal  = render_device.create_texture2d("media/textures/leaf_normal.png");
      leaf_diffuse.set_min_filter(TextureFilter_LinearMipLinear);
      leaf_diffuse.set_mag_filter(TextureFilter_Linear);
      leaf_normal.set_min_filter(TextureFilter_LinearMipLinear);
      leaf_normal.set_mag_filter(TextureFilter_Linear);
      leaf_render_material.set_shader_tags("leaf"); // dedicated leaf pass (textured, no alpha discard)
      TextureList leaf_textures = leaf_render_material.textures();
      leaf_textures.insert("diffuseTexture", leaf_diffuse);
    }

    materials.insert(DROPLET_HULL_MATERIAL, droplet_material);
    materials.insert(DROPLET_FLUID_MATERIAL, droplet_fluid_material);
    materials.insert(SKY_MATERIAL, sky_material);
    materials.insert(WATER_SURFACE_MATERIAL_NAME, water_material);
    materials.insert("flower", flower_material);
    materials.insert("leaf", leaf_render_material);

      //scale meshes

    scale_model(leaf_model, LEAVES_SCALE);
    scale_model(plant_model, PLANT_SCALE);

      //create leaves

      //the static demo tree (add_stem) is replaced by one procedural plant that GROWS from a sprout;
      //its leaves are spawned as real leaf-blade physics bodies as it grows (see spawn_initial_plant).
     //add_stem(STEAM_POSITION, to_quat(math::rotate(math::degree(90.0f), math::vec3f(0.0f, 1.0f, 0.0f))));

      //configure physics

    //dynamics_world->setGravity(btVector3(0, -10, 0));
    dynamics_world->setGravity(btVector3(0, -15, 0));

    droplet_particle_shape.reset(new btSphereShape(btScalar(DROPLET_PARTICLE_RADIUS)));
    static_bind_shape.reset(new btSphereShape(btScalar(0.01f)));

    btVector3 bt_local_inertia(0, 0, 0);
    droplet_particle_shape->calculateLocalInertia(DROPLET_PARTICLE_MASS, bt_local_inertia);
    droplet_particle_local_intertia = math::vec3f(bt_local_inertia.getX(), bt_local_inertia.getY(), bt_local_inertia.getZ());

    //droplet_particle_shape->setMargin(COLLISION_MARGIN);

    sky = scene::Mesh::create();
    
    sky->set_mesh(media::geometry::MeshFactory::create_sphere(SKY_MATERIAL, SKY_RADIUS, math::vec3f(0.0f)));
    sky->bind_to_parent(*scene_root);

    setup_ground();
    setup_fireflies(scene_renderer);
    spawn_initial_plant();

    if (!gContactAddedCallback)
    {
      gContactAddedCallback = contact_added_callback;
    }
  }

  void setup_fireflies(SceneRenderer& scene_renderer)
  {
    static const float TWO_PI = 6.2831853f;
    MaterialList materials = scene_renderer.materials();

    for (size_t i = 0; i < FIREFLY_COUNT; i++)
    {
      Firefly f;

      std::string name = "firefly_" + std::to_string(i);

      f.material.set_shader_tags("firefly");
      PropertyMap fprops = f.material.properties();
      fprops.set("glowColor", math::vec3f(0.0f)); // start invisible
      materials.insert(name.c_str(), f.material);

      f.body = scene::Mesh::create();
      f.body->set_mesh(media::geometry::MeshFactory::create_sphere(name.c_str(), FIREFLY_RADIUS));
      f.body->bind_to_parent(*scene_root);

      f.light = scene::PointLight::create();
      f.light->set_light_color(FIREFLY_COLOR);
      f.light->set_attenuation(LIGHTS_ATTENUATION);
      f.light->set_intensity(0.0f);
      f.light->set_range(FIREFLY_LIGHT_RANGE);
      f.light->bind_to_parent(*scene_root);

      float ang = frand() * TWO_PI;
      float rad = FIREFLY_SPAWN_RADIUS * sqrt(frand());
      f.base = math::vec3f(cos(ang) * rad, 0.0f, sin(ang) * rad);
      f.time_offset   = frand() * 100.0f;
      f.lifetime      = crand(9.0f, 17.0f);
      f.pulse_speed   = crand(1.5f, 3.5f);
      f.pulse_phase   = frand() * TWO_PI;
      f.drift_speed_x = crand(0.25f, 0.6f);
      f.drift_speed_z = crand(0.25f, 0.6f);
      f.drift_phase_x = frand() * TWO_PI;
      f.drift_phase_z = frand() * TWO_PI;

      fireflies.push_back(f);
    }
  }

  void update_fireflies()
  {
    float t = last_frame_time / float(CLOCKS_PER_SEC);

    for (Firefly& f : fireflies)
    {
      float life = (t + f.time_offset) / f.lifetime;
      life = life - floor(life);                                  // 0..1 rise cycle

      float y = FIREFLY_BOTTOM + (FIREFLY_TOP - FIREFLY_BOTTOM) * life;
      float x = f.base.x + sin(t * f.drift_speed_x + f.drift_phase_x) * FIREFLY_DRIFT;
      float z = f.base.z + sin(t * f.drift_speed_z + f.drift_phase_z) * FIREFLY_DRIFT;

      float pulse = 0.45f + 0.55f * sin(t * f.pulse_speed + f.pulse_phase);
      float fade  = 1.0f;                                         // appear/disappear via transparency
      if (life < 0.12f)      fade = life / 0.12f;
      else if (life > 0.85f) fade = (1.0f - life) / 0.15f;

      float glow = pulse * fade;
      if (glow < 0.0f) glow = 0.0f;

      math::vec3f pos(x, y, z);
      f.body->set_position(pos);
      PropertyMap fprops = f.material.properties();
      fprops.set("glowColor", FIREFLY_COLOR * glow);
      f.light->set_position(pos);
      f.light->set_intensity(FIREFLY_LIGHT_INTENSITY * glow);
    }
  }

  void scale_model(media::geometry::Model& model, const math::vec3f& scale)
  {
    media::geometry::Vertex* vertex = model.mesh.vertices_data();

    for (size_t i=0, count=model.mesh.vertices_count(); i<count; i++, vertex++)
    {
      vertex->position *= scale;
    }
  }

  void load_materials(const media::geometry::Model& model, MaterialList& materials, Device& render_device)
  {
    for (size_t i=0, count=model.mesh.primitives_count(); i<count; i++)
    {
      const media::geometry::Primitive& primitive = model.mesh.primitive(i);
      media::geometry::Material* asset_material = model.materials.find(primitive.material.c_str());

      if (materials.find(primitive.material.c_str()))
        continue;

      Material render_material;
      TextureList render_textures = render_material.textures();

      render_material.set_properties(asset_material->properties());

      for (size_t j=0, count=asset_material->textures_count(); j<count; j++)
      {
        const media::geometry::Texture& asset_texture = asset_material->get_texture(j);
        Texture texture = render_device.create_texture2d(asset_texture.file_name.c_str());

        texture.set_min_filter(TextureFilter_LinearMipLinear);
        texture.set_mag_filter(TextureFilter_Linear);

        render_textures.insert(asset_texture.name.c_str(), texture);
      }

      materials.insert(primitive.material.c_str(), render_material);
    }
  }

  void setup_ground()
  {
      //graphics

    scene::Mesh::Pointer floor = scene::Mesh::create();

    // the visible platform extends far past the physics ground so it reads as a plane reaching the horizon
    const float FLOOR_VISUAL_SIZE = GROUND_SIZE * 6.0f;
    media::geometry::Mesh floor_mesh = media::geometry::MeshFactory::create_box("mtl1", FLOOR_VISUAL_SIZE, 0.01f, FLOOR_VISUAL_SIZE);

    // tile the brick texture so it stays detailed across the enlarged platform instead of stretching
    {
      const float tiles = FLOOR_VISUAL_SIZE / GROUND_SIZE;
      media::geometry::Vertex* fv = floor_mesh.vertices_data();
      for (size_t i=0, c=floor_mesh.vertices_count(); i<c; i++, fv++)
        fv->tex_coord *= tiles;
    }

    floor->set_mesh(floor_mesh);
    floor->set_reflection_excluded(true); // submerged platform must not occlude the mirror camera in the water reflection
    floor->bind_to_parent(*scene_root);

      //physics

    ground_shape.reset(new btBoxShape(btVector3(btScalar(GROUND_SIZE), btScalar(0.1f), btScalar(GROUND_SIZE))));

    btTransform ground_transform;
    ground_transform.setIdentity();
    ground_transform.setOrigin(btVector3(0, GROUND_OFFSET, 0));

    phys_bodies.push_back(std::make_shared<PhysBodySync>(ground_shape, 0.f, math::vec3f(0.0f), math::vec3f(0, GROUND_OFFSET, 0), math::quatf(), floor, COLLISION_GROUP_GROUND, COLLISION_MASK_GROUND, dynamics_world));

    phys_bodies.back()->body->setUserPointer(&ground_rigid_body_info);

      //configure water surface

    water_surface.mesh_node->bind_to_parent(*scene_root);
  }

  void add_stem(const math::vec3f& position, const math::quatf& rotation)
  {
      //create leaves

    for (size_t i=0, count=leaf_model.mesh.primitives_count(); i<count; i++)
    {
      using namespace media::geometry;

      const media::geometry::Primitive& primitive = leaf_model.mesh.primitive(i);

      if (primitive.type != PrimitiveType_TriangleList)
        continue;;

      scene::Mesh::Pointer mesh = scene::Mesh::create();

      mesh->set_mesh(leaf_model.mesh, i, 1);
      mesh->set_position(position);
      mesh->set_orientation(rotation);
      mesh->bind_to_parent(*scene_root);

      if (primitive.name.find("leave_") == 0)
      {
          //primitive name starts from "leaf"

        engine_log_debug("leaf found '%s'", primitive.name.c_str());

        std::shared_ptr<btCollisionShape> shape;

        if (auto* entry = convex_shapes.find(primitive.name))
          shape = *entry;

        if (!shape)
        {
          engine_log_debug("create phys mesh shape '%s'", primitive.name.c_str());

          std::vector<math::vec3f> vertices;
          std::vector<media::geometry::Mesh::index_type> indices;
          std::unordered_map<media::geometry::Mesh::index_type, media::geometry::Mesh::index_type> index_map;

          vertices.reserve(leaf_model.mesh.vertices_count());
          indices.reserve(primitive.count * 3);

          const auto* index = leaf_model.mesh.indices_data() + primitive.first * 3;

          for (size_t i=0, count=primitive.count * 3; i<count; i++, index++)
          {
            size_t new_index = 0;
            if (index_map.find(*index) == index_map.end())
            {
              index_map[*index] = vertices.size();
              new_index = vertices.size();
              vertices.push_back(leaf_model.mesh.vertices_data()[*index].position);
            }
            else
            {
              new_index = index_map[*index];
            }

            indices.push_back(new_index);
          }

          // A leaf is a thin, roughly-convex blade. Use a convex hull instead of btBvhTriangleMeshShape:
          // the triangle-mesh shape is static-only (wrong for a dynamic body) and yields no usable inertia,
          // whereas a hull collides correctly AND gives a proper anisotropic inertia tensor (see below).
          btConvexHullShape* hull = new btConvexHullShape();

          const math::vec3f* vertex = &vertices[0];
          for (size_t i=0, vertices_count=vertices.size(); i<vertices_count; i++, vertex++)
            hull->addPoint(btVector3((*vertex)[0], (*vertex)[1], (*vertex)[2]), false);

          hull->recalcLocalAabb();

          engine_log_debug("leaf convex hull shape '%s' (%u points)", primitive.name.c_str(), vertices.size());

          shape = std::shared_ptr<btCollisionShape>(hull);

          //shape->setMargin(COLLISION_MARGIN);

          convex_shapes.insert(primitive.name.c_str(), shape);
        }

          //create leaf

        // proper inertia from the hull (thin in the leaf-normal axis -> anisotropic), instead of a hardcoded
        // isotropic (1,1,1)*mass that made a flat leaf tumble like a uniform sphere
        btVector3 bt_local_inertia(0, 0, 0);
        shape->calculateLocalInertia(LEAF_MASS, bt_local_inertia);
        math::vec3f local_inertia(bt_local_inertia.getX(), bt_local_inertia.getY(), bt_local_inertia.getZ());

        phys_bodies.push_back(std::make_shared<PhysBodySync>(shape, LEAF_MASS, local_inertia, position, rotation, mesh, COLLISION_GROUP_LEAF, COLLISION_MASK_LEAF, dynamics_world));

          //find pivot point for constraint

        math::vec3f pivot_point(0, 0, 0);
        float nearest_distance = 1.e06f;

        for (size_t j=0, count=leaf_model.mesh.primitives_count(); j<count; j++)
        {
          const media::geometry::Primitive& primitive2 = leaf_model.mesh.primitive(j);

          if (&primitive == &primitive2)
            continue;

          if (primitive2.type != PrimitiveType_TriangleList)
            continue;

          if (primitive2.name.find("leaf_") != 0)
            continue;

          find_nearest_point(leaf_model.mesh, primitive, primitive2, pivot_point, nearest_distance);
        }

          //find leaf center

        const Vertex* vertices = leaf_model.mesh.vertices_data() + primitive.base_vertex;
        const media::geometry::Mesh::index_type* index = leaf_model.mesh.indices_data() + primitive.first * 3;
        size_t indices_count = primitive.count * 3;
        math::vec3f initial_center = 0.0f;
      
        for (size_t i=0; i<indices_count; i++, index++)
        {
          initial_center += vertices[*index].position;
        }

        initial_center /= indices_count;
        math::vec3f local_center = initial_center;             // body-local mesh centroid (before placement)
        initial_center  = rotation * initial_center + position; // world centroid at init

        Leaf leaf(last_frame_time, *this);

          //no per-leaf lights: the tree is lit by the soft moonlight (the flying spot) instead, to
          //avoid the over-lit/bioluminescent look and keep a soft mysterious night mood.

          //configure leaf constraint

        leaf.phys_body = phys_bodies.back();
        leaf.target_transform = leaf.phys_body->body->getWorldTransform();
        leaf.initial_center = initial_center;
        leaf.local_center   = local_center;

        leaf.phys_body->body->setUserPointer(leaf.rigid_body_info.get());

        leaf.phys_body->body->setFriction(crand(LEAF_MIN_FRICTION, LEAF_MAX_FRICTION));

        bt_local_inertia = btVector3(0, 0, 0);

        btTransform start_transform;
        start_transform.setIdentity();

        btVector3 static_body_bind_pos = btVector3(pivot_point[0], pivot_point[1], pivot_point[2]);
        start_transform.setOrigin(static_body_bind_pos);

        start_transform = leaf.phys_body->body->getWorldTransform() * start_transform;

        btRigidBody::btRigidBodyConstructionInfo rb_info(0.0f, nullptr, static_bind_shape.get(), bt_local_inertia);
        rb_info.m_startWorldTransform = start_transform;
        leaf.static_bind_body = std::make_shared<btRigidBody>(rb_info);

        //pure constraint anchor: give it no user pointer, so register it to collide with nothing
        //(mask 0) — otherwise a droplet touching it fires the contact callback with a null RigidBodyInfo
        dynamics_world->addRigidBody(leaf.static_bind_body.get(), COLLISION_GROUP_LEAF, 0);

        btVector3 static_bind_anchor(0, 0, 0);
        btVector3 leaf_anchor(pivot_point[0], pivot_point[1], pivot_point[2]);

        leaf.constraint = std::make_shared<btPoint2PointConstraint>(*leaf.phys_body->body, *leaf.static_bind_body,
          leaf_anchor, static_bind_anchor);

        dynamics_world->addConstraint(leaf.constraint.get(), true);

        leaves.push_back(leaf);
      }
    }
  }

  // Quaternion rotating unit vector a onto unit vector b.
  math::quatf quat_from_to(const math::vec3f& a, const math::vec3f& b)
  {
    math::vec3f an = math::normalize(a), bn = math::normalize(b);
    float d = math::dot(an, bn);
    if (d > 0.9999f)
      return to_quat(math::radian(0.0f), math::vec3f(0, 1, 0));
    if (d < -0.9999f)
    {
      math::vec3f perp = std::fabs(an.x) < 0.9f ? math::vec3f(1, 0, 0) : math::vec3f(0, 1, 0);
      return to_quat(math::radian(math::constf::pi), math::normalize(math::cross(an, perp)));
    }
    return to_quat(math::radian(acos(d)), math::normalize(math::cross(an, bn)));
  }

  // Orient a leaf blade so it lies roughly FLAT: its broad face (normal = local +Y) points UP with a
  // small random tilt, and its long axis (local +X) points horizontally outward along `dir`. Leaves
  // then face the sky/rain instead of standing on edge.
  math::quatf leaf_orientation(const math::vec3f& dir)
  {
      //leaf normal = up + a small random tilt (~14 deg) for diversity
    math::vec3f n = math::normalize(math::vec3f(crand() * 0.18f, 1.0f, crand() * 0.18f));

      //long axis = horizontal outward component of the branch's leaf direction, made perpendicular to n
    math::vec3f f(dir.x, 0.0f, dir.z);
    if (math::qlen(f) < 1e-4f) f = math::vec3f(crand(), 0.0f, crand());
    f = f - n * math::dot(f, n);
    if (math::qlen(f) < 1e-4f) f = math::vec3f(1, 0, 0);
    f = math::normalize(f);

    math::quatf q1 = quat_from_to(math::vec3f(1, 0, 0), f); // +X -> outward
    math::vec3f n1 = q1 * math::vec3f(0, 1, 0);
    float c = math::dot(n1, n); c = c < -1.f ? -1.f : (c > 1.f ? 1.f : c);
    float s = math::dot(math::cross(n1, n), f);
    math::quatf q2 = to_quat(math::radian(atan2(s, c)), f); // twist +Y -> n (face up)
    return q2 * q1;
  }

  // Spawn one PROCEDURAL leaf (petiole "leg" + textured blade, generated per-seed for diversity) as a
  // physics body pinned at its leg base. The collision is a CONCAVE V-trough COMPOUND (so droplets
  // collect in the leaf instead of rolling off a convex hull). `length` = blade length in world units;
  // the geometry is already at that size with the attach point at the origin.
  void spawn_leaf(const math::vec3f& world_pos, const math::quatf& rotation, float length, uint32_t seed)
  {
    media::geometry::Mesh leaf_mesh;
    launcher::generate_leaf(leaf_mesh, seed, length, "leaf");

    uint32_t lvc = leaf_mesh.vertices_count();
    if (lvc == 0)
      return;

      //blade centroid (for droplet spawn aim)
    const media::geometry::Vertex* lv = leaf_mesh.vertices_data();
    math::vec3f centroid(0.0f);
    for (uint32_t i = 0; i < lvc; i++)
      centroid += lv[i].position;
    centroid /= (float) lvc;

      //compound collision: a row of convex flaps forming a concave V-channel (see generate_leaf_collision)
    std::vector<std::vector<math::vec3f>> pieces;
    launcher::generate_leaf_collision(seed, length, pieces);

    btCompoundShape* compound = new btCompoundShape();
    std::vector<std::shared_ptr<btCollisionShape>> children;
    btTransform child_tm;
    child_tm.setIdentity();
    for (size_t pi = 0; pi < pieces.size(); pi++)
    {
      const std::vector<math::vec3f>& piece = pieces[pi];
      if (piece.size() < 4)
        continue;
      btConvexHullShape* ch = new btConvexHullShape();
      for (size_t k = 0; k < piece.size(); k++)
        ch->addPoint(btVector3(piece[k][0], piece[k][1], piece[k][2]), false);
      ch->recalcLocalAabb();
      children.push_back(std::shared_ptr<btCollisionShape>(ch));
      compound->addChildShape(child_tm, ch);
    }
    if (children.empty())
    {
      delete compound;
      return;
    }
    std::shared_ptr<btCollisionShape> shape(compound);

    btVector3 bt_inertia(0, 0, 0);
    shape->calculateLocalInertia(LEAF_MASS, bt_inertia);
    math::vec3f local_inertia(bt_inertia.x(), bt_inertia.y(), bt_inertia.z());

      //textured render node. The leaf grows in via the RENDER node scale only (collision stays full
      //size -- scaling a compound's children is awkward, and the brief grow-in mismatch is harmless).
    scene::Mesh::Pointer mesh = scene::Mesh::create();
    mesh->set_mesh(leaf_mesh);
    mesh->set_position(world_pos);
    mesh->set_orientation(rotation);
    mesh->set_scale(math::vec3f(LEAF_GROW_START));
    mesh->bind_to_parent(*scene_root);

    phys_bodies.push_back(std::make_shared<PhysBodySync>(shape, LEAF_MASS, local_inertia, world_pos, rotation, mesh, COLLISION_GROUP_LEAF, COLLISION_MASK_LEAF, dynamics_world));

    Leaf leaf(last_frame_time, *this);
    leaf.phys_body = phys_bodies.back();
    leaf.hull_children = children; // keep the compound's child shapes alive for the leaf's lifetime
    leaf.target_transform = leaf.phys_body->body->getWorldTransform();
    leaf.local_center   = centroid;                          // blade centre (for droplet spawn aim)
    leaf.initial_center = rotation * centroid + world_pos;
    leaf.phys_body->body->setUserPointer(leaf.rigid_body_info.get());
    leaf.phys_body->body->setFriction(crand(LEAF_MIN_FRICTION, LEAF_MAX_FRICTION));
    // no gravity on generated leaves: they're pinned only at the spine base, so gravity would swing
    // the blade down off the branch. Zeroing it keeps the leaf in its (normal-up) spawn pose; droplet
    // impacts still nudge it (it springs back via the leaf return-force in update()).
    leaf.phys_body->body->setGravity(btVector3(0, 0, 0));

      //pin at the spine base (the body origin) so the leaf mounts there and swings from its petiole
    btVector3 pivot(0.0f, 0.0f, 0.0f);
    btTransform start;
    start.setIdentity();
    start.setOrigin(pivot);
    start = leaf.phys_body->body->getWorldTransform() * start;

    btVector3 anchor_inertia(0, 0, 0);
    btRigidBody::btRigidBodyConstructionInfo rb_info(0.0f, nullptr, static_bind_shape.get(), anchor_inertia);
    rb_info.m_startWorldTransform = start;
    leaf.static_bind_body = std::make_shared<btRigidBody>(rb_info);
    dynamics_world->addRigidBody(leaf.static_bind_body.get(), COLLISION_GROUP_LEAF, 0);

    leaf.constraint = std::make_shared<btPoint2PointConstraint>(*leaf.phys_body->body, *leaf.static_bind_body,
      pivot, btVector3(0, 0, 0));
    dynamics_world->addConstraint(leaf.constraint.get(), true);

      //leaf growth: ramp scale 0->1 over a random duration (diversity). Geometry is already world-size
      //so full node scale is 1.
    leaf.full_scale    = 1.0f;
    leaf.grow_age      = 0.0f;
    leaf.grow_duration = crand(LEAF_GROW_MIN_SECONDS, LEAF_GROW_MAX_SECONDS);

    leaves.push_back(leaf);
  }

  // Grow each freshly-spawned leaf in: ramp its render-node scale and collision-hull scale from
  // LEAF_GROW_START up to full over its (random) grow_duration. Called once per frame.
  void update_leaf_growth(float dt)
  {
    for (Leaf& leaf : leaves)
    {
      if (leaf.grow_duration <= 0.0f || leaf.grow_age >= leaf.grow_duration)
        continue;

      leaf.grow_age += dt;
      float t  = leaf.grow_age / leaf.grow_duration;
      if (t > 1.0f) t = 1.0f;
      float e  = t * t * (3.0f - 2.0f * t);                 // smoothstep ease
      float f  = LEAF_GROW_START + (1.0f - LEAF_GROW_START) * e;

      leaf.phys_body->mesh->set_scale(math::vec3f(leaf.full_scale * f)); // visual grow-in only
    }
  }

  // One growing plant at the scene centre, sprouting from g=0 (see update_plants for the growth).
  void spawn_initial_plant()
  {
    // root the plant AT the water surface so it emerges from the water and its mirror reflection
    // meets it at the waterline (previously the plant floated ~6 units above the water -> detached
    // reflection that read as a broken mirror).
    generate_plant(math::vec3f(STEAM_POSITION.x, WATER_LEVEL, STEAM_POSITION.z));

#if PLANT_DEBUG_FULLGROWN
    {
      // headless visual test: jump the plant to full growth + leaves at startup (the sim barely
      // advances under Chrome's virtual time, so growth wouldn't otherwise be visible in a screenshot)
      std::shared_ptr<Plant> pl = plants.back();
      pl->growth = 1.0f;
      pl->age = PLANT_GROW_SECONDS;
      rebuild_plant(pl);
      for (size_t i = 0; i < pl->slots.size(); i++)
      {
        const launcher::LeafSlot& slot = pl->slots[i];
        math::vec3f wpos = pl->base_position + slot.pos * pl->scale;
        spawn_leaf(wpos, leaf_orientation(slot.dir), slot.size * 3.0f, slot.seed);
        pl->slot_spawned[i] = 1;
      }
      update_leaf_growth(100.0f); // grow the leaves in via the real path
    }
#endif
  }

  // Remove the oldest n droplet particles (front of droplet_particles == generation order) from both
  // droplet_particles and the master phys_bodies, so ~PhysBodySync removes them from the Bullet world.
  void retire_oldest_droplet_particles(size_t n)
  {
    if (n > droplet_particles.size())
      n = droplet_particles.size();

    if (!n)
      return;

    std::vector<PhysBodySync*> retiring;
    retiring.reserve(n);

    for (size_t i = 0; i < n; i++)
      retiring.push_back(droplet_particles[i].get());

    droplet_particles.erase(droplet_particles.begin(), droplet_particles.begin() + n);

    phys_bodies.erase(std::remove_if(phys_bodies.begin(), phys_bodies.end(),
      [&](const std::shared_ptr<PhysBodySync>& b) {
        return std::find(retiring.begin(), retiring.end(), b.get()) != retiring.end();
      }), phys_bodies.end());
  }

  void generate_droplet()
  {
    if (!leaves.size())
      return;

    if (last_droplet_generated_time && last_frame_time - last_droplet_generated_time < DROPLET_GENERATION_INTERVAL)
      return;

    // Keep the total particle count bounded by recycling the OLDEST particles, rather than stopping
    // generation. The old behaviour (skip when over MAX_PARTICLES_COUNT) let particles pile up to the
    // cap and then stalled all new droplets, leaving the top leaf permanently empty after a while.
    const size_t per_droplet = (size_t) std::max(1, live.particles_per_droplet);

    if (droplet_particles.size() + per_droplet > MAX_PARTICLES_COUNT)
      retire_oldest_droplet_particles(droplet_particles.size() + per_droplet - MAX_PARTICLES_COUNT);

    last_droplet_generated_time = last_frame_time;

    // Spawn just above ONE of the HIGHEST leaves (random), so the droplet lands on that top leaf and
    // travels down the plant to the bottom.
    float y_hi = -1e30f, y_lo = 1e30f;
    for (Leaf& lf : leaves)
    {
      float y = lf.phys_body->body->getWorldTransform().getOrigin().y();
      if (y > y_hi) y_hi = y;
      if (y < y_lo) y_lo = y;
    }
    float band = y_hi - 0.15f * (y_hi - y_lo); // only the highest ~15% of leaves

    std::vector<size_t> top_leaves;
    for (size_t i = 0; i < leaves.size(); i++)
      if (leaves[i].phys_body->body->getWorldTransform().getOrigin().y() >= band)
        top_leaves.push_back(i);

    size_t leaf_index = top_leaves.empty()
      ? 0
      : top_leaves[(size_t) (frand() * top_leaves.size()) % top_leaves.size()];
    Leaf& leaf = leaves[leaf_index];

    // a little bit above the leaf's CURRENT centroid (the leaf tilts/moves over time, so use the live
    // pose) -> the cluster falls onto the leaf and runs off it downward.
    btVector3 lc(leaf.local_center[0], leaf.local_center[1], leaf.local_center[2]);
    btVector3 c = leaf.phys_body->body->getWorldTransform() * lc;
    math::vec3f spawn(c.x(), c.y() + DROPLET_SPAWN_ABOVE_LEAF, c.z());

    generate_droplet(spawn);
  }

  void generate_droplet(const math::vec3f& droplet_center)
  {
    float friction_factor = crand(DROPLET_MIN_FRICTION_FACTOR, DROPLET_MAX_FRICTION_FACTOR);

    // (re)build the per-particle collision shape at the current physical radius, so new droplets pick up
    // the live "physical radius" slider. Existing particles keep their own shape (shared_ptr) -> stable.
    droplet_particle_shape.reset(new btSphereShape(btScalar(live.physical_radius)));
    btVector3 bt_local_inertia(0, 0, 0);
    droplet_particle_shape->calculateLocalInertia(DROPLET_PARTICLE_MASS, bt_local_inertia);
    droplet_particle_local_intertia = math::vec3f(bt_local_inertia.getX(), bt_local_inertia.getY(), bt_local_inertia.getZ());

    // generate exactly live.particles_per_droplet particles, distributed over concentric shells in a
    // small ball whose radius scales with the physical radius (was DROPLET_RADIUS/8 = physical*2.5).
    const int   target      = std::max(1, live.particles_per_droplet);
    const float ball_radius = live.physical_radius * 20.0f / 8.0f;
    const size_t per_shell  = PARALLELS_COUNT * MERIDIANS_COUNT;
    const size_t layers     = (target + per_shell - 1) / per_shell;

    static const float PI2 = 3.1415926f * 2.0f;
    int made = 0;

    for (size_t i = 0; i < layers && made < target; i++)
    {
      float radius = float(i + 1) / layers * ball_radius;

      for (size_t j = 0; j < PARALLELS_COUNT && made < target; j++)
      {
        float angle1 = float(j) / PARALLELS_COUNT * PI2;
        float y      = 2.0f * (cos(angle1) - 0.5f) * radius;

        for (size_t k = 0; k < MERIDIANS_COUNT && made < target; k++)
        {
          float angle2 = float(k) / MERIDIANS_COUNT * PI2;
          math::vec3f position(cos(angle2) * radius, y, sin(angle2) * radius);
          generate_droplet_particle(position + droplet_center, friction_factor);
          made++;
        }
      }
    }
  }

  void generate_droplet_particle(const math::vec3f& offset, float friction_factor)
  {
    scene::Mesh::Pointer mesh; // particles are never rendered unless debug-drawing -> don't allocate/sync a scene mesh

    if (DROPLET_DEBUG_DRAW)
    {
      mesh = scene::Mesh::create();
      mesh->set_mesh(droplet_debug_particle_mesh);
      mesh->bind_to_parent(*scene_root);
    }

    phys_bodies.push_back(std::make_shared<PhysBodySync>(droplet_particle_shape, DROPLET_PARTICLE_MASS, droplet_particle_local_intertia, offset, math::quatf(), mesh, COLLISION_GROUP_DROPLET, COLLISION_MASK_DROPLET, dynamics_world));

    PhysBodySync& particle = *phys_bodies.back();

    particle.body->setUserPointer(&droplet_rigid_body_info);
    particle.body->setFriction(crand(DROPLET_PARTICLE_MIN_FRICTION, DROPLET_PARTICLE_MAX_FRICTION) * friction_factor);
    //particle.body->setSleepingThresholds(DROPLET_PARTICLE_LINEAR_SLEEPING_THRESHOLD, DROPLET_PARTICLE_ANGULAR_SLEEPING_THRESHOLD);
    //particle.body->setAngularFactor(btVector3(0.0f, 0.0f, 0.0f));

    particle.droplet_particle = std::make_shared<DropletParticle>();

    droplet_particles.push_back(phys_bodies.back());
  }

  // (Re)build a plant's branch geometry for its current growth and push it to the GPU.
  void rebuild_plant(const std::shared_ptr<Plant>& plant)
  {
    media::geometry::Mesh mesh;
    launcher::generate_plant_mesh(mesh, plant->params, plant->growth);
    if (mesh.primitives_count() > 0)
      plant->mesh->set_mesh(mesh);
    plant->built_growth = plant->growth;
  }

  // Once the plant is fully grown, build a PHYSICS SKELETON: one kinematic rigid body per branch, baked
  // into world space. From here the branch mesh follows the bodies (rebuild_skeleton_mesh). Phase 1 keeps
  // the bodies kinematic at rest -> renders identically; later phases make them dynamic + jointed (wind/drag).
  void finalize_skeleton(const std::shared_ptr<Plant>& plant)
  {
    std::vector<launcher::Bone> bones;
    launcher::collect_bones(plant->params, bones);

    const float s = plant->scale;
    const math::vec3f base = plant->base_position;

    plant->bones.clear();
    plant->bones.reserve(bones.size());

    for (size_t i = 0; i < bones.size(); i++)
    {
      const launcher::Bone& src = bones[i];

      BoneBody bb;
      bb.parent  = src.parent;
      bb.indices = src.indices;
      bb.verts   = src.verts;
      for (size_t v = 0; v < bb.verts.size(); v++)
        bb.verts[v].position = bb.verts[v].position * s;     // bone-local, scaled to world units

      math::vec3f origin_world = base + src.rest_base * s;    // bone origin = the joint to its parent

      btConvexHullShape* hull = new btConvexHullShape();
      for (size_t v = 0; v < bb.verts.size(); v++)
        hull->addPoint(btVector3(bb.verts[v].position.x, bb.verts[v].position.y, bb.verts[v].position.z), false);
      hull->recalcLocalAabb();
      bb.shape = std::shared_ptr<btCollisionShape>(hull);

      btTransform t;
      t.setIdentity();
      t.setOrigin(btVector3(origin_world.x, origin_world.y, origin_world.z));
      bb.motion = std::make_shared<btDefaultMotionState>(t);

      bool is_root = (bb.parent < 0);

        //root = fixed anchor (mass 0). other bones = dynamic, jointed to the parent by a spring.
      float mass = 0.0f;
      btVector3 inertia(0, 0, 0);
      if (!is_root)
      {
        float lw = math::length(src.rest_tip - src.rest_base) * s;
        float rw = src.radius * s;
        mass = 40.0f * rw * rw * lw;
        mass = mass < 0.05f ? 0.05f : (mass > 8.0f ? 8.0f : mass);
        hull->calculateLocalInertia(mass, inertia);
      }

      bb.radius_world = src.radius * s;
      bb.mass         = mass;

      btRigidBody::btRigidBodyConstructionInfo ci(mass, bb.motion.get(), hull, inertia);
      bb.body = std::make_shared<btRigidBody>(ci);
      bb.body->setActivationState(DISABLE_DEACTIVATION);
      if (!is_root)
        bb.body->setGravity(btVector3(0, 0, 0)); // springs hold the rest pose; wind/drag move it
      dynamics_world->addRigidBody(bb.body.get(), COLLISION_GROUP_LEAF, 0); // mask 0: skeleton-internal, collides with nothing

      plant->bones.push_back(bb);

        //spring joint to the parent at this bone's base (lock translation, allow limited bending)
      if (!is_root)
      {
        BoneBody& self = plant->bones.back();
        btRigidBody* parent_body = plant->bones[self.parent].body.get();
        math::vec3f parent_origin = base + bones[self.parent].rest_base * s;

        btTransform frameA; frameA.setIdentity();
        frameA.setOrigin(btVector3(origin_world.x - parent_origin.x,
                                   origin_world.y - parent_origin.y,
                                   origin_world.z - parent_origin.z));
        btTransform frameB; frameB.setIdentity();

        btGeneric6DofSpringConstraint* spring =
          new btGeneric6DofSpringConstraint(*parent_body, *self.body.get(), frameA, frameB, true);
        spring->setLinearLowerLimit(btVector3(0, 0, 0));
        spring->setLinearUpperLimit(btVector3(0, 0, 0));   // no stretch
        float lim = JOINT_ANGLE_LIMIT;
        spring->setAngularLowerLimit(btVector3(-lim, -lim, -lim));
        spring->setAngularUpperLimit(btVector3( lim,  lim,  lim));
        float k = joint_stiffness_for(bb); // mass^2-scaled: structural joints >> twig joints
        for (int a = 3; a < 6; a++)
        {
          spring->enableSpring(a, true);
          spring->setStiffness(a, k);
          spring->setDamping(a, live.joint_damping);
          spring->setEquilibriumPoint(a, 0.0f);
        }
        self.joint = std::shared_ptr<btTypedConstraint>(spring);
        dynamics_world->addConstraint(spring, true);
      }
    }

      //reattach each leaf to its NEAREST branch bone (was pinned to a static anchor), so leaves follow
      //the swaying branches and dragging a leaf pulls its branch -> propagates down to the root.
    for (Leaf& leaf : leaves)
    {
      if (leaf.on_skeleton)
        continue;
      btVector3 lp = leaf.phys_body->body->getWorldTransform().getOrigin();
      int best = -1;
      float bestd = 1e30f;
      for (size_t b = 0; b < bones.size(); b++)
      {
        math::vec3f mid = base + (bones[b].rest_base + bones[b].rest_tip) * 0.5f * s;
        float d = (lp - btVector3(mid.x, mid.y, mid.z)).length2();
        if (d < bestd) { bestd = d; best = (int) b; }
      }
      if (best < 0)
        continue;

      btRigidBody* bone_body = plant->bones[best].body.get();
      if (leaf.constraint)       dynamics_world->removeConstraint(leaf.constraint.get());
      if (leaf.static_bind_body) dynamics_world->removeRigidBody(leaf.static_bind_body.get());

      btVector3 bone_origin = bone_body->getWorldTransform().getOrigin();
      btVector3 pivot_in_bone = lp - bone_origin; // bone rest rotation is identity
      leaf.constraint = std::make_shared<btPoint2PointConstraint>(*leaf.phys_body->body, *bone_body,
        btVector3(0, 0, 0), pivot_in_bone);
      dynamics_world->addConstraint(leaf.constraint.get(), true);

      leaf.on_skeleton   = true;
      leaf.skeleton_bone = bone_body;
      leaf.rest_local    = bone_body->getWorldTransform().inverse() * leaf.target_transform; // pose relative to the bone
    }

    plant->skeletonized = true;
    plant->mesh->set_position(math::vec3f(0.0f)); // the rebuilt skeleton mesh is already in world space
    plant->mesh->set_scale(math::vec3f(1.0f));
    rebuild_skeleton_mesh(plant);
  }

  // Rebuild the branch mesh from the live bone-body transforms (the skeleton drives the geometry).
  void rebuild_skeleton_mesh(const std::shared_ptr<Plant>& plant)
  {
    std::vector<media::geometry::Vertex>           verts;
    std::vector<media::geometry::Mesh::index_type> indices;

    for (size_t b = 0; b < plant->bones.size(); b++)
    {
      const BoneBody& bb = plant->bones[b];
      if (verts.size() + bb.verts.size() > 64000)
        break; // uint16 index budget

      const btTransform& T = bb.body->getWorldTransform();
      const btMatrix3x3& R = T.getBasis();
      const btVector3&   O = T.getOrigin();
      uint32_t base_idx = (uint32_t) verts.size();

      for (size_t v = 0; v < bb.verts.size(); v++)
      {
        const math::vec3f& lp = bb.verts[v].position;
        const math::vec3f& ln = bb.verts[v].normal;
        btVector3 wp = T * btVector3(lp.x, lp.y, lp.z);
        btVector3 wn = R * btVector3(ln.x, ln.y, ln.z);
        media::geometry::Vertex o = bb.verts[v];
        o.position = math::vec3f(wp.x(), wp.y(), wp.z());
        o.normal   = math::vec3f(wn.x(), wn.y(), wn.z());
        verts.push_back(o);
      }
      (void) O;
      for (size_t k = 0; k < bb.indices.size(); k++)
        indices.push_back((media::geometry::Mesh::index_type) (base_idx + bb.indices[k]));
    }

    if (verts.empty())
      return;

    media::geometry::Mesh mesh;
    mesh.add_primitive("flower", media::geometry::PrimitiveType_TriangleList,
      &verts[0], (media::geometry::Mesh::index_type) verts.size(), &indices[0], (uint32_t) indices.size());
    plant->mesh->set_mesh(mesh);
  }

  // Joint spring stiffness for a bone: scales with mass^2 so a thick structural joint (carrying a big
  // subtree) is ~1000x stiffer than a twig joint, instead of the ~10x a radius term gives. That keeps
  // the trunk/main limbs from folding under wind while leaving the twigs springy. Tuned live.
  float joint_stiffness_for(const BoneBody& bb) const
  {
    return live.joint_stiffness * (bb.mass * bb.mass + 0.0008f);
  }

  // Push the branch skeleton with OSCILLATING wind so the tree sways back and forth (rather than a
  // one-way shove that folds it downwind). drive swings through zero; a per-bone phase desyncs branches.
  void apply_wind(const std::shared_ptr<Plant>& plant)
  {
    float t = wind_time;
    float swell = 0.7f + 0.3f * std::sin(t * 0.30f); // slow breeze envelope (always positive)
    math::vec3f dir = math::normalize(math::vec3f(1.0f, 0.0f, 0.35f));

    for (size_t b = 0; b < plant->bones.size(); b++)
    {
      BoneBody& bb = plant->bones[b];
      if (bb.parent < 0 || bb.body->getInvMass() <= 0.0f)
        continue;
      float m    = 1.0f / bb.body->getInvMass();
      float ph   = (float) b * 0.7f;
      float sway = std::sin(t * 1.6f + ph) + 0.4f * std::sin(t * 3.3f + ph * 1.7f);
      float drive = swell * (0.30f + 0.9f * sway); // oscillates ± -> sway, with a slight downwind bias
      math::vec3f f = dir * (m * live.wind_accel * drive);
      bb.body->applyCentralForce(btVector3(f.x, f.y, f.z));
      bb.body->activate(true);
    }
  }

  // Push the live stiffness/damping sliders into the existing spring joints (so the branches can be
  // tuned from stiff to springy without a rebuild). Cheap: ~one downcast + 3 axes per branch.
  void update_joint_params(const std::shared_ptr<Plant>& plant)
  {
    for (size_t b = 0; b < plant->bones.size(); b++)
    {
      BoneBody& bb = plant->bones[b];
      btGeneric6DofSpringConstraint* spring = dynamic_cast<btGeneric6DofSpringConstraint*>(bb.joint.get());
      if (!spring)
        continue;
      float k = joint_stiffness_for(bb);
      for (int a = 3; a < 6; a++)
      {
        spring->setStiffness(a, k);
        spring->setDamping(a, live.joint_damping);
      }
    }
  }

  // Advance every still-growing plant by dt; re-mesh when growth moved enough (called each frame).
  void update_plants(float dt)
  {
    wind_time += dt;

    for (auto& plant : plants)
    {
      if (!plant->skeletonized)
      {
        if (plant->growth < 1.0f)
        {
          plant->age   += dt;
          plant->growth = std::min(1.0f, plant->age / PLANT_GROW_SECONDS);
          rebuild_plant(plant); // rebuild every frame while growing -> smooth, continuous growth
        }
      }
      else
      {
        update_joint_params(plant); // live stiffness/damping sliders
        apply_wind(plant);
        rebuild_skeleton_mesh(plant); // branch mesh follows the physics skeleton
      }

        //spawn a real leaf-blade physics body for each slot whose birth has been reached
      for (size_t i = 0; i < plant->slots.size(); i++)
      {
        if (plant->slot_spawned[i])
          continue;

        const launcher::LeafSlot& slot = plant->slots[i];

        if (plant->growth < slot.birth_g)
          continue;

        // slot.pos is local (scaled into the world by plant->scale); slot.size is already a world
        // length. 2x bigger than the previous leaves, per request.
        math::vec3f wpos = plant->base_position + slot.pos * plant->scale;
        float world_len  = slot.size * 3.0f;

        spawn_leaf(wpos, leaf_orientation(slot.dir), world_len, slot.seed);
        plant->slot_spawned[i] = 1;
      }

        //fully grown -> build the physics skeleton (after all leaves have spawned)
      if (!plant->skeletonized && plant->growth >= 1.0f)
        finalize_skeleton(plant);
    }
  }

  void generate_plant()
  {
      //water accumulated -> sprout a new plant (growth itself is driven by time, see update_plants)

    if (plants.size() >= PLANT_MAX_COUNT)
      return;

    math::vec3f position(crand() * PLANT_GENERATION_RADIUS + PLANT_SAFE_ZONE_RADIUS, PLANT_GENERATION_HEIGHT, crand() * PLANT_GENERATION_RADIUS + PLANT_SAFE_ZONE_RADIUS);

    generate_plant(position);
  }

  void generate_plant(const math::vec3f& position)
  {
    std::shared_ptr<Plant> plant = std::make_shared<Plant>();

      //fresh random shape/colour each app run, then sprout at g=0 (it grows in over time)
    uint32_t seed = ((uint32_t) (frand() * 4294967040.0f)) ^ 0x9e3779b9u;
    plant->params        = launcher::make_plant_params(seed);
    plant->base_position = position;
    plant->age           = 0.0f;
    plant->growth        = 0.0f;

      //size the plant so its MATURE structure (branches stack past the trunk) reaches target_height.
      //Measured once from a full-growth build; the node scale then holds while the geometry grows in.
    media::geometry::Mesh full;
    launcher::generate_plant_mesh(full, plant->params, 1.0f);
    float full_height = 0.0f;
    for (uint32_t i = 0, n = full.vertices_count(); i < n; i++)
      full_height = std::max(full_height, full.vertices_data()[i].position.y);
    plant->scale = full_height > 1e-3f ? plant->params.target_height / full_height : 1.0f;

      //leaf attachment slots (each spawns a physics leaf when growth passes its birth_g)
    launcher::collect_leaf_slots(plant->params, plant->slots);
    plant->slot_spawned.assign(plant->slots.size(), 0);

    plant->mesh = scene::Mesh::create();

    plant->mesh->set_position(position);
    plant->mesh->set_scale(math::vec3f(plant->scale));
    rebuild_plant(plant);
    plant->mesh->bind_to_parent(*scene_root);

    plants.push_back(plant);
  }

  int last_substeps = 0; // number of fixed physics substeps run this frame (drives the water at the same rate)

  // Pull live droplet knobs from the in-page sliders (window.DROPLET.*) once per frame; each falls back
  // to its compile-time constant when the slider/page hasn't set it. No-op off the web.
  void refresh_live_tuning()
  {
#ifdef __EMSCRIPTEN__
    live.metaball_radius = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.metaballRadius != null) ? window.DROPLET.metaballRadius : $0; }, (double) DROPLET_RAYMARCH_PARTICLE_RADIUS);
    live.influence       = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.influence      != null) ? window.DROPLET.influence      : $0; }, (double) DROPLET_INFLUENCE_RADIUS);
    live.iso             = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.iso            != null) ? window.DROPLET.iso            : $0; }, (double) DROPLET_ISO_THRESHOLD);
    live.force           = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.force          != null) ? window.DROPLET.force          : $0; }, (double) DROPLET_SURFACE_TENSION);
    live.damping         = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.damping        != null) ? window.DROPLET.damping        : $0; }, (double) DROPLET_VISCOSITY);
    live.cohesion_radius = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.cohesionRadius != null) ? window.DROPLET.cohesionRadius : $0; }, (double) DROPLET_COHESION_RADIUS);
    live.particles_per_droplet = EM_ASM_INT({ return (window.DROPLET && window.DROPLET.particlesPerDroplet != null) ? (window.DROPLET.particlesPerDroplet | 0) : $0; }, 20);
    live.physical_radius = (float) EM_ASM_DOUBLE({ return (window.DROPLET && window.DROPLET.physicalRadius  != null) ? window.DROPLET.physicalRadius  : $0; }, (double) DROPLET_PARTICLE_RADIUS);
    live.wind_accel      = (float) EM_ASM_DOUBLE({ return (window.WIND && window.WIND.accel      != null) ? window.WIND.accel      : $0; }, (double) WIND_ACCEL);
    live.joint_stiffness = (float) EM_ASM_DOUBLE({ return (window.WIND && window.WIND.stiffness  != null) ? window.WIND.stiffness  : $0; }, (double) JOINT_STIFFNESS_BASE);
    live.joint_damping   = (float) EM_ASM_DOUBLE({ return (window.WIND && window.WIND.damping    != null) ? window.WIND.damping    : $0; }, (double) JOINT_DAMPING);
#endif
  }

  // SPH-style surface tension (Akinci et al. 2013): for every near pair of particles within a droplet,
  // a COHESION force pulls them together with a kernel that is zero at contact and at the cohesion
  // radius h and peaks in between (so the cluster minimises surface area without imploding), plus a
  // VISCOSITY force that damps only their RELATIVE velocity (internal jiggle settles, bulk fall kept).
  // Bullet's sphere collisions provide the short-range repulsion. O(n^2) per cluster; clustering keeps
  // n modest and the particle budget caps the worst case.
  void apply_droplet_surface_tension()
  {
    const float h = live.cohesion_radius;
    if (h <= 1.0e-4f)
      return;
    const float h2    = h * h;
    const float gamma = live.force;   // cohesion strength (surface tension)
    const float visc  = live.damping; // relative-velocity damping

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      std::vector<std::shared_ptr<PhysBodySync>>& b = droplet->bodies;
      const size_t n = b.size();
      if (n < 2)
        continue;

      st_pos.clear(); st_vel.clear();
      st_pos.reserve(n); st_vel.reserve(n);
      st_acc.assign(n, btVector3(0.0f, 0.0f, 0.0f));
      for (size_t i = 0; i < n; i++)
      {
        st_pos.push_back(b[i]->body->getWorldTransform().getOrigin());
        st_vel.push_back(b[i]->body->getLinearVelocity());
      }

      // each unordered pair once (j>i); the pair's accel is equal-and-opposite (equal masses), so apply
      // +to i and -to j -> half the work of the naive i!=j double loop, same result.
      for (size_t i = 0; i < n; i++)
      {
        const btVector3& pi = st_pos[i];
        const btVector3& vi = st_vel[i];
        for (size_t j = i + 1; j < n; j++)
        {
          btVector3 d  = pi - st_pos[j];
          float     r2 = d.length2();
          if (r2 >= h2 || r2 < 1.0e-10f)
            continue;
          float r   = std::sqrt(r2);
          float x   = r / h;                    // 0..1
          float w   = 4.0f * x * (1.0f - x);    // cohesion kernel: 0 at contact & at h, peak mid
          btVector3 dir = d / r;                // from j to i
          btVector3 ai  = dir * (-gamma * w)              // cohesion: pull i toward j
                        + (st_vel[j] - vi) * (visc * w);  // viscosity: match neighbour velocity
          st_acc[i] += ai;
          st_acc[j] -= ai;                      // Newton's 3rd law (equal masses)
        }
      }

      // st_acc is an ACCELERATION; multiply by the particle mass so the tiny mass (0.002) doesn't blow
      // it up. gamma/visc therefore read as accelerations, independent of the mass value.
      for (size_t i = 0; i < n; i++)
      {
        float inv_m = b[i]->body->getInvMass();
        float m     = inv_m > 0.0f ? 1.0f / inv_m : DROPLET_PARTICLE_MASS;
        b[i]->body->applyCentralForce(st_acc[i] * m);
        b[i]->body->activate(true);
      }
    }
  }

  // Metaball-raymarch surface update for one droplet: position+scale the proxy box to enclose the
  // particle cluster, and upload the particle field (centres+radius) as per-node shader uniforms.
  // Replaces the convex-hull build for raymarch droplets; the cubemap reflection/refraction is
  // unchanged (rendered from the droplet centre as before).
  void update_droplet_raymarch(const std::shared_ptr<Droplet>& droplet)
  {
    if (!droplet->hull_mesh || droplet->points.empty())
      return;

    std::vector<math::vec4f> particles;
    particles.reserve(MAX_DROPLET_RAYMARCH_PARTICLES);

      //evenly sample up to the shader's fixed array size from the (now denser) cluster.
      //spreads MAX picks across the whole set, so a 100-particle droplet actually uses all 64
      //(the old stride-by-ceil only used ~50 of 100).

    size_t count = droplet->points.size();
    size_t used  = count < MAX_DROPLET_RAYMARCH_PARTICLES ? count : MAX_DROPLET_RAYMARCH_PARTICLES;

    float max_dist = 0.0f;

    for (size_t k = 0; k < used; k++)
    {
      size_t i = (count <= MAX_DROPLET_RAYMARCH_PARTICLES) ? k : (k * count) / used;
      const math::vec3f& p = droplet->points[i];
      particles.push_back(math::vec4f(p[0], p[1], p[2], live.metaball_radius));
      max_dist = std::max(max_dist, length(p - droplet->center));
    }

    int particle_count = (int)particles.size();

      //pad to the shader array size; padding entries are never sampled (the shader breaks at particleCount)

    particles.resize(MAX_DROPLET_RAYMARCH_PARTICLES, math::vec4f(0.0f));

    float box_half = (max_dist + live.metaball_radius + live.influence) * DROPLET_RAYMARCH_BOX_MARGIN;

    droplet->hull_mesh->set_position(droplet->center);
    droplet->hull_mesh->set_scale(math::vec3f(box_half));

    if (!DROPLET_REFLECT_SKYBOX)
      droplet->hull_mesh->set_environment_map_local_point(math::vec3f(0.0f)); // node sits at the centre -> dynamic cubemap eye = centre

    common::PropertyMap props;
    props.set("particles", particles);
    props.set("particleCount", particle_count);
    props.set("dropletCenter", droplet->center);
    props.set("influenceRadius", live.influence);
    props.set("isoThreshold", live.iso);
    props.set("boxHalfExtent", box_half);

    droplet->hull_mesh->set_user_data(props);
  }

  void update(float dt)
  {
    last_frame_time = clock();

    // keep the skybox centred on the camera so it reads as infinitely far (no parallax during movement)
    sky->set_position(math::vec3f(camera->world_tm() * math::vec4f(0.0f, 0.0f, 0.0f, 1.0f)));

    refresh_live_tuning(); // pull droplet knobs from the in-page sliders (web)

      //debug dump

    if (last_frame_time - last_debug_dump_time > DEBUG_DUMP_INTERVAL)
    {
      last_debug_dump_time = last_frame_time;
      engine_log_debug("Droplets count: %d (particles count %d)", droplets.size(), droplet_particles.size());
    }

      //step the simulation with the real frame time (clamped to avoid a spiral of death), advancing
      //by a fixed 1/60 substep so behaviour is frame-rate independent (was hardcoded 1/60 per frame ->
      //0.5x speed at 30fps, 2x at 120Hz). Bullet's substep count then drives the water at the same rate.

    float clamped_dt = dt < (1.f / 4.f) ? dt : (1.f / 4.f);

    last_substeps = dynamics_world->stepSimulation(clamped_dt, 10, 1.f / 60.f);

      //generate droplets

    generate_droplet();

      //advance procedural plant growth (time-driven branch growth) + leaf unfurl

    update_plants(clamped_dt);
    update_leaf_growth(clamped_dt);

      //update leaves

    for (Leaf& leaf : leaves)
    {
      if (leaf.on_skeleton && leaf.skeleton_bone)
        // the leaf's rest pose tracks its (swaying) branch bone, so the spring restores it relative to
        // the branch rather than to a fixed world pose; droplet/wind nudges still spring back.
        leaf.target_transform = leaf.skeleton_bone->getWorldTransform() * leaf.rest_local;

      btRigidBody* body = leaf.phys_body->body.get();
      float inv_mass = body->getInvMass();
      float mass = inv_mass == 0.0f ? 0.0f : 1.0f / inv_mass;

      static const float TIME_STEP = 0.025f;
      static const float FORCE_FACTOR = 0.05f;
      static const float TORQUE_FACTOR = 0.01f;

      btVector3 linear_velocity, angular_velocity;

      btTransformUtil::calculateVelocity(body->getWorldTransform(), leaf.target_transform, TIME_STEP,
        linear_velocity, angular_velocity);

      linear_velocity -= body->getLinearVelocity();
      angular_velocity -= body->getAngularVelocity();

      btVector3 force = mass * linear_velocity / TIME_STEP * FORCE_FACTOR,
                torque = mass * angular_velocity / TIME_STEP * TORQUE_FACTOR;

      body->applyCentralForce(force);
      body->applyTorque(torque);
    }

      //remove fallen droplets

    for (std::shared_ptr<PhysBodySync>& particle : droplet_particles)
    {
      if (particle->body->getWorldTransform().getOrigin().getY() >= MIN_DROPLET_PARTICLE_HEIGHT)
        continue;

      if (particle->droplet_particle->fallen)
        continue;

      particle->droplet_particle->fallen = true;

      fallen_droplet_particles_count++;

        //a droplet reaching the water surface no longer disturbs it (no impact ripple)
    }

    droplet_particles.erase(std::remove_if(droplet_particles.begin(), droplet_particles.end(), [](const std::shared_ptr<PhysBodySync>& particle) {
      return particle->body->getWorldTransform().getOrigin().getY() < MIN_DROPLET_PARTICLE_HEIGHT;
    }), droplet_particles.end());

      //and drop them from the master list too, so ~PhysBodySync removes the rigid body from the Bullet world
      //(only droplet particles; ground/leaf bodies have no droplet_particle and are left untouched)

    phys_bodies.erase(std::remove_if(phys_bodies.begin(), phys_bodies.end(), [](const std::shared_ptr<PhysBodySync>& body) {
      return body->droplet_particle && body->droplet_particle->fallen;
    }), phys_bodies.end());

      //clusterize droplet particles to droplets

    float cluster_radius = live.physical_radius * 20.0f; // was DROPLET_RADIUS (= physical * 20)

    for (size_t i=0; i<CLUSTERIZE_STEPS_COUNT; i++)
    {
        //clear clusters

      for (std::shared_ptr<Droplet>& droplet : droplets)
      {
        droplet->points.clear();
        droplet->bodies.clear(); // was never cleared -> bodies accumulated across passes/frames, ramping cohesion force and leaking refs
      }

        //clusterization step

      for (std::shared_ptr<PhysBodySync>& particle : droplet_particles)
      {
        btVector3 bt_position = particle->body->getWorldTransform().getOrigin();
        math::vec3f position(bt_position.getX(), bt_position.getY(), bt_position.getZ());
        bool added = false;

        for (std::shared_ptr<Droplet>& droplet : droplets)
        {
          float distance = length(droplet->center - position);

          if (distance < cluster_radius)
          {
            droplet->points.push_back(position);
            droplet->bodies.push_back(particle);

            droplet->center = math::vec3f(0.0f, 0.0f, 0.0f);

            for (const math::vec3f& point : droplet->points)
              droplet->center += point;

            droplet->center /= droplet->points.size();

            added = true;
            break;
          }
        }

        if (!added)
        {
          std::shared_ptr<Droplet> droplet = std::make_shared<Droplet>();

          droplet->center = position;
          droplet->points.push_back(position);
          droplet->bodies.push_back(particle);
          
          droplets.push_back(droplet);
        }
      }

      size_t fake_droplets = 0;

      for (std::shared_ptr<Droplet>& droplet : droplets)
      {
        if (droplet->points.size() < MIN_DROPLET_PARTICLES_COUNT && droplet->remove_counter != 0)
          fake_droplets++;
      }

      size_t normal_droplets = droplets.size() - fake_droplets;

//      engine_log_debug("step=%d, radius=%f, droplets=%d/%d", i, cluster_radius, normal_droplets, droplets.size());

      if (normal_droplets <= PREFERRED_MAX_DROPLETS_COUNT)
        break;

        //cleanup clusterization step

      droplets.erase(std::remove_if(droplets.begin(), droplets.end(), [](const std::shared_ptr<Droplet>& droplet) {
        return droplet->hull_mesh == nullptr;
      }), droplets.end());

      cluster_radius *= CLUSTERIZE_STEP_FACTOR;
    }

      //create hulls

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      if (droplet->hull_mesh)
        continue;

      droplet->hull_mesh = scene::Mesh::create();

      // per-droplet dynamic env-map prerendering; skipped when reflecting the static skybox (saves the
      // whole-scene cubemap re-render per droplet).
      if (!DROPLET_REFLECT_SKYBOX)
        droplet->hull_mesh->set_environment_map_required(true);

      // proxy box (unit cube [-1,1]); positioned at the centre + scaled to enclose the metaball each frame.
      // The fragment shader raymarches the particle SDF inside it; the cube itself is never seen.
      droplet->hull_mesh->set_mesh(media::geometry::MeshFactory::create_box(DROPLET_FLUID_MATERIAL, 2.f, 2.f, 2.f));

      droplet->point_light = scene::PointLight::create();

      droplet->point_light->set_light_color(math::vec3f(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY)));
      droplet->point_light->set_attenuation(LIGHTS_ATTENUATION);
      droplet->point_light->set_intensity(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY));
      droplet->point_light->set_range(crand(LIGHTS_MIN_RANGE, LIGHTS_MAX_RANGE));
      droplet->point_light->set_position(math::vec3f(0, 0.2, 0));

      if (!DROPLET_DEBUG_DRAW)
      {
        droplet->hull_mesh->bind_to_parent(*scene_root);
        //droplet->point_light->bind_to_parent(*droplet->hull_mesh); //note: hull mesh is in world coords, point light should be moved separately
      }

        //find largest droplet

      Droplet* largest_droplet = nullptr;

      for (auto& droplet : droplets)
      {
        if (!largest_droplet || droplet->points.size() > largest_droplet->points.size())
          largest_droplet = droplet.get();
      }

        //synchronize camera

      if (largest_droplet)
      {
        math::vec3f center = largest_droplet->center;

        static float TARGET_DISTANCE = 2;
        static float POSITION_INTERPOLATION_FACTOR = 0.0001f;
        static float TARGET_INTERPOLATION_FACTOR = 0.00001f;

        math::vec3f required_camera_position = center + normalize(math::vec3f(1, 0.5, 0)) * TARGET_DISTANCE;
        math::vec3f required_camera_target = center;
        math::vec3f current_camera_position = math::vec3f(camera->world_tm() * math::vec4f(0, 0, 0, 1));
        math::vec3f current_camera_target = current_camera_position + math::vec3f(camera->world_tm() * math::vec4f(0, 1, 0, 0));
        math::vec3f new_camera_position = current_camera_position + (required_camera_position - current_camera_position) * POSITION_INTERPOLATION_FACTOR;
        math::vec3f new_camera_target = current_camera_target + (required_camera_target - current_camera_target) * TARGET_INTERPOLATION_FACTOR;

        //camera->set_position(inverse(camera->parent()->world_tm()) * new_camera_position);
        //camera->world_look_to(-new_camera_target, math::vec3f(0, 1, 0));
      }
    }

    //configure droplets

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      math::vec3f center(0.0f);

      if (droplet->points.empty())
      {
        droplet->center = center;
        droplet->prev_centers.push_back(center);
        continue;
      }
      
      for (const math::vec3f& point : droplet->points)
      {
        center += point;
      }
      
      droplet->center = center / droplet->points.size();
      droplet->prev_centers.push_back(droplet->center);
    }

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      if (droplet->prev_centers.size() > DROPLET_CENTER_APPROXIMATION_STEPS_COUNT)
        droplet->prev_centers.pop_front();

      math::vec3f center(0.0f);

      for (const math::vec3f& prev_center : droplet->prev_centers)
        center += prev_center;

      center /= droplet->prev_centers.size();

      droplet->center = center;
    }

    //remove empty droplets

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      if (droplet->points.size() < MIN_DROPLET_PARTICLES_COUNT)
      {
        droplet->remove_counter++;

        if (!DROPLET_DEBUG_DRAW)
          droplet->hull_mesh->unbind();
      }
      else
      {
        if (!DROPLET_DEBUG_DRAW)
          droplet->hull_mesh->bind_to_parent(*scene_root);
        droplet->remove_counter = 0;
      }
    }

      //a single procedural plant grows from startup (see spawn_initial_plant / update_plants); the old
      //water-triggered multi-plant spawning is disabled. Keep the droplet-landing chime.

    if (fallen_droplet_particles_count > PLANT_FALLEN_DROPLET_PARTICLES_COUNT_THRESHOLD)
    {
      SoundPlayer::play_sound(SoundId::droplet_ground);
      fallen_droplet_particles_count = 0;
    }

    droplets.erase(std::remove_if(droplets.begin(), droplets.end(), [](const std::shared_ptr<Droplet>& droplet) { return droplet->remove_counter > DROPLET_REMOVE_COUNTER_THRESHOLD; }),
      droplets.end());

    //build droplet surfaces (metaball raymarch)

    for (std::shared_ptr<Droplet>& droplet : droplets)
      update_droplet_raymarch(droplet);

    //surface tension: SPH-style PAIRWISE cohesion + viscosity between neighbouring particles within
    //each droplet (Akinci et al. 2013). Cohesion attracts near pairs with a kernel that is 0 at contact
    //and at the cohesion radius h and peaks in between -> the blob minimises surface area and holds
    //together (necking/merging) WITHOUT collapsing toward a point (the old centroid spring did the
    //latter, which read as wrong). Viscosity damps only the RELATIVE velocity of neighbours, so internal
    //jiggle settles while the droplet's bulk fall is preserved. Bullet's sphere collisions supply the
    //short-range repulsion, so equilibrium spacing sits near contact.

    apply_droplet_surface_tension();

      //sync bodies with scene

    for (std::shared_ptr<PhysBodySync>& particle : phys_bodies)
    {
      if (!particle->mesh) // invisible droplet particle (debug draw off) -> nothing to sync
        continue;

      btTransform transform;

      if (particle->body && particle->body->getMotionState())
      {
        particle->body->getMotionState()->getWorldTransform(transform);
      }
      else
      {
        transform = particle->body->getWorldTransform();
      }

      particle->mesh->set_position(math::vec3f(transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ()));
      particle->mesh->set_orientation(math::quatf(transform.getRotation().getX(), transform.getRotation().getY(), transform.getRotation().getZ(), transform.getRotation().getW()));

	  //engine_log_debug("world pos object = %f,%f,%f\n", float(transform.getOrigin().getX()), float(transform.getOrigin().getY()), float(transform.getOrigin().getZ())); 
    }

      //move point lights for droplets

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      if (droplet->point_light)
        droplet->point_light->set_position(droplet->center);
    }

      //configure plant lights

    for (std::shared_ptr<Plant>& plant : plants)
    {
      math::vec3f plant_center = plant->mesh->position();
      std::pair<int, int> light_zone(plant_center.x / PLANT_LIGHT_ZONE_SIZE, plant_center.z / PLANT_LIGHT_ZONE_SIZE);
      std::shared_ptr<PlantLight> plant_light;
      auto it = plant_lights.find(light_zone);

      if (it != plant_lights.end())
      {
        plant_light = it->second;
      }
      else
      {
        plant_light = std::make_shared<PlantLight>();

        plant_light->point_light = scene::PointLight::create();

        plant_light->point_light->set_light_color(math::vec3f(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY)));
        plant_light->point_light->set_attenuation(LIGHTS_ATTENUATION);
        plant_light->point_light->set_intensity(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY));
        //plant_light->point_light->set_range(PLANT_LIGHT_RANGE_FACTOR * crand(LIGHTS_MIN_RANGE, LIGHTS_MAX_RANGE));
        plant_light->point_light->set_range(PLANT_LIGHT_RANGE_FACTOR * crand(LIGHTS_MIN_RANGE, LIGHTS_MAX_RANGE));

        engine_log_debug("Point light for zone %d,%d created\n", light_zone.first, light_zone.second);

        plant_light->point_light->set_position(math::vec3f(light_zone.first * PLANT_LIGHT_ZONE_SIZE, PLANT_LIGHT_HEIGHT, light_zone.second * PLANT_LIGHT_ZONE_SIZE));
        plant_light->point_light->bind_to_parent(*scene_root);

        plant_lights[light_zone] = plant_light;
      }
    }

      //play sound for interactions between droplets and leaves

    if (clock() - last_leaf_contact_sound_played_time > PLAY_CONTACT_SOUND_IF_NO_CONTACTS_DURING)
    {
      if (leaves_collisions_count >= PLAY_CONTACT_SOUND_COLLISIONS_COUNT)
      {
        SoundPlayer::play_sound(SoundId::droplet_leaf);

        engine_log_debug("Droplet-leaf contact sound played");

        leaves_collisions_count = 0;
        last_leaf_contact_sound_played_time = clock();
      }
    }

      //update water surface

    for (int s = 0; s < last_substeps; ++s) // run the wave/swell sim once per fixed physics substep -> real-time, fps-independent
      water_surface.update();

      //update fireflies

    update_fireflies();
  }

  /// Input control
  void inputGrab(float ray_start_x, float ray_start_y, float ray_start_z, float ray_end_x, float ray_end_y, float ray_end_z)
  {
    //do bullet physics raycast to find leaf under click
    btVector3 ray_start(ray_start_x, ray_start_y, ray_start_z), ray_end(ray_end_x, ray_end_y, ray_end_z);
    btCollisionWorld::ClosestRayResultCallback raycast_callback(ray_start, ray_end);

    raycast_callback.m_collisionFilterMask = COLLISION_GROUP_LEAF;

    dynamics_world->rayTest(ray_start, ray_end, raycast_callback);

    if (raycast_callback.m_collisionObject != 0)
    {
      //calculate object local position
      btVector3 local_hit_pos = raycast_callback.m_collisionObject->getWorldTransform().inverse() * raycast_callback.m_hitPointWorld;

//      engine_log_info("Raycast hit object at %f %f %f", raycast_callback.m_hitPointWorld.getX(), raycast_callback.m_hitPointWorld.getY(), raycast_callback.m_hitPointWorld.getZ());
//      engine_log_info("Raycast hit object at local pos at %f %f %f", local_hit_pos.getX(), local_hit_pos.getY(), local_hit_pos.getZ());

      //We use only btRigidBody for collision objects, so safe to use static_cast here
      grabbed_object = static_cast<btRigidBody*>(const_cast<btCollisionObject*>(raycast_callback.m_collisionObject));
      grabbed_object_pos_world = raycast_callback.m_hitPointWorld;
      grabbed_object_pos_local = local_hit_pos;
    }
/*    else
    {
      engine_log_info("Raycast hit nothing");
    }*/
  }

  void inputDrag(float target_offset_x, float target_offset_y, float target_offset_z)
  {
    if (!grabbed_object)
      return;

    //calculate distance from current grabbed position to target position
    btVector3 target_world_pos = grabbed_object_pos_world + btVector3(target_offset_x, target_offset_y, target_offset_z);
    btVector3 current_world_pos = grabbed_object->getWorldTransform() * grabbed_object_pos_local;
    btVector3 delta = target_world_pos - current_world_pos;
    btVector3 force = delta * DRAG_FORCE_MULTIPLIER;

    //limit force to max force
    if (force.length2() > DRAG_MAX_FORCE * DRAG_MAX_FORCE)
      force = force.normalize() * DRAG_MAX_FORCE;

    grabbed_object->activate(true);
    grabbed_object->applyForce(force, current_world_pos - grabbed_object->getCenterOfMassPosition());

/*    engine_log_info("grabbed_object_pos_world %.2f %.2f %.2f", grabbed_object_pos_world.getX(), grabbed_object_pos_world.getY(), grabbed_object_pos_world.getZ());
    engine_log_info("current_world_pos %.2f %.2f %.2f", current_world_pos.getX(), current_world_pos.getY(), current_world_pos.getZ());
    engine_log_info("target_offset %.2f %.2f %.2f", target_offset_x, target_offset_y, target_offset_z);
    engine_log_info("target_world_pos %.2f %.2f %.2f", target_world_pos.getX(), target_world_pos.getY(), target_world_pos.getZ());
    engine_log_info("delta %.2f %.2f %.2f", delta.getX(), delta.getY(), delta.getZ());
    engine_log_info("applyForce %.2f %.2f %.2f!!!", (float)(delta.getX() * DRAG_FORCE_MULTIPLIER), (float)(delta.getY() * DRAG_FORCE_MULTIPLIER), (float)(delta.getZ() * DRAG_FORCE_MULTIPLIER)); */
  }

  void inputRelease()
  {
    grabbed_object = 0;
  }
};

World::World(scene::Node::Pointer scene_root, SceneRenderer& scene_render, const Camera::Pointer& camera)
  : impl(new Impl(scene_root, scene_render, camera))
{

}

World::~World()
{
}

void World::update(float dt)
{
  impl->update(dt);
}

/// Input control
void World::inputGrab(float ray_start_x, float ray_start_y, float ray_start_z, float ray_end_x, float ray_end_y, float ray_end_z)
{
  impl->inputGrab(ray_start_x, ray_start_y, ray_start_z, ray_end_x, ray_end_y, ray_end_z);
}

void World::inputDrag(float target_offset_x, float target_offset_y, float target_offset_z)
{
  impl->inputDrag(target_offset_x, target_offset_y, target_offset_z);
}

void World::inputRelease()
{
  impl->inputRelease();
}
