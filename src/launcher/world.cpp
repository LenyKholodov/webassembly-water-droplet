#include "shared.h"

#include <common/log.h>
#include <common/named_dictionary.h>
#include <math/utility.h>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"

#include "hull/hull.h"

#include <list>

using namespace engine::common;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::scene;
using namespace engine;

const char* LEAF_MESH = "media/meshes/leaf.obj";
const float DROPLET_PARTICLE_RADIUS = 0.05f;
const float DROPLET_PARTICLE_MASS = 0.001f;
const float DROPLET_RADIUS = DROPLET_PARTICLE_RADIUS * 20.0f;
const bool DROPLET_DEBUG_DRAW = false;
const float DROPLET_PARTICLE_LINEAR_SLEEPING_THRESHOLD = 1.f;
const float DROPLET_PARTICLE_ANGULAR_SLEEPING_THRESHOLD = 1.f;
const size_t DROPLET_CENTER_APPROXIMATION_STEPS_COUNT = 5;
const size_t MIN_DROPLET_PARTICLES_COUNT = 3;
const float MIN_DROPLET_PARTICLE_HEIGHT = -6.f;
const size_t DROPLET_REMOVE_COUNTER_THRESHOLD = 200;
static size_t PARALLELS_COUNT = 7, MERIDIANS_COUNT = 7, LAYERS_COUNT = 1;
const size_t MIN_PARTICLES_COUNT = PARALLELS_COUNT * MERIDIANS_COUNT * LAYERS_COUNT / 3;
const math::vec3f LEAVES_SCALE(0.1f);
const float LEAF_MASS = 1.0f;
const math::vec3f STEAM_POSITION(-3.0f, 0, 3.0f);

const float GROUND_SIZE = 50.0f;
const float GROUND_OFFSET = -7.f;

const char* DROPLET_HULL_MATERIAL = "droplet";
const int COLLISION_GROUP_DROPLET = 1;
const int COLLISION_GROUP_GROUND = 1 << 1;
const int COLLISION_GROUP_LEAF = 1 << 2;
const int COLLISION_MASK_DROPLET = COLLISION_GROUP_GROUND | COLLISION_GROUP_LEAF | COLLISION_GROUP_DROPLET;
const int COLLISION_MASK_GROUND = COLLISION_GROUP_DROPLET;
const int COLLISION_MASK_LEAF = COLLISION_GROUP_DROPLET;
const float DRAG_FORCE_MULTIPLIER = 10.f;
const float DRAG_MAX_FORCE = 2.f;

const float COLLISION_MARGIN = 0.001f;
const float FRICTION = 0.9;

const float LIGHTS_MIN_INTENSITY = 0.35f;
const float LIGHTS_MAX_INTENSITY = 1;
const float LIGHTS_MIN_RANGE = 3.5;
const float LIGHTS_MAX_RANGE = 4.5;
const math::vec3f LIGHTS_ATTENUATION(1, 0.75, 0.25);

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

struct PhysBodySync
{
  std::shared_ptr<btDiscreteDynamicsWorld> dynamics_world;
  std::shared_ptr<btCollisionShape> shape;
  std::shared_ptr<btDefaultMotionState> motion_state;
  std::shared_ptr<btRigidBody> body;
  scene::Mesh::Pointer mesh;

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
    : mesh(mesh)
    , shape(shape)
  {
    btTransform start_transform;
    start_transform.setIdentity();
    start_transform.setOrigin(btVector3(position[0], position[1], position[2]));
    start_transform.setRotation(btQuaternion(rotation[0], rotation[1], rotation[2], rotation[3]));

    motion_state = std::make_shared<btDefaultMotionState>(start_transform);

    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state.get(), shape.get(), btVector3(local_intertia[0], local_intertia[1], local_intertia[2]));
    body = std::make_shared<btRigidBody>(rb_info);

    dynamics_world->addRigidBody(body.get(), collision_group, collision_mask);
  }

  ~PhysBodySync()
  {
    dynamics_world->removeRigidBody(body.get());
  }
};

struct Leaf
{
  std::shared_ptr<PhysBodySync> phys_body;
  std::shared_ptr<btRigidBody> static_bind_body;
  std::shared_ptr<btTypedConstraint> constraint;
  btTransform target_transform;
  math::vec3f initial_center;
  scene::PointLight::Pointer point_light;
};

struct Droplet
{
  math::vec3f center;
  std::list<math::vec3f> prev_centers;
  std::vector<math::vec3f> points;
  std::vector<std::shared_ptr<PhysBodySync>> bodies;
  HullBuilder hull_builder;
  scene::Mesh::Pointer hull_mesh;
  size_t remove_counter = 0;
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

}

struct World::Impl
{
  media::geometry::Model leaf_model;
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
  std::shared_ptr<btRigidBody> ground_body;
  std::vector<std::shared_ptr<PhysBodySync>> phys_bodies;
  media::geometry::Mesh droplet_debug_particle_mesh;
  math::vec3f droplet_particle_local_intertia;
  common::NamedDictionary<std::shared_ptr<btCollisionShape>> convex_shapes;
  std::vector<Leaf> leaves;
  std::vector<std::shared_ptr<PhysBodySync>> droplet_particles;
  std::vector<std::shared_ptr<Droplet>> droplets;
  Material droplet_material;
  btRigidBody* grabbed_object;
  btVector3 grabbed_object_pos_world;
  btVector3 grabbed_object_pos_local;

  Impl(scene::Node::Pointer scene_root, SceneRenderer& scene_renderer, const scene::Camera::Pointer& camera)
    : leaf_model(media::geometry::MeshFactory::load_obj_model(LEAF_MESH))
    , scene_root(scene_root)
    , camera(camera)
    , collision_configuration(new btDefaultCollisionConfiguration())
    , dispatcher(new btCollisionDispatcher(collision_configuration.get()))
    , broadphase(new btDbvtBroadphase())
    , solver(new btSequentialImpulseConstraintSolver())
    , dynamics_world(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collision_configuration.get()))
    , droplet_debug_particle_mesh(media::geometry::MeshFactory::create_sphere("mtl1", DROPLET_PARTICLE_RADIUS))
    , grabbed_object(0)
  {
      //load materials

    Device render_device = scene_renderer.device();
    MaterialList materials = scene_renderer.materials();

    for (size_t i=0, count=leaf_model.mesh.primitives_count(); i<count; i++)
    {
      const media::geometry::Primitive& primitive = leaf_model.mesh.primitive(i);
      media::geometry::Material* asset_material = leaf_model.materials.find(primitive.material.c_str());

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

      //configure droplet material

    droplet_material.set_shader_tags("fresnel");
    droplet_material.set_textures(materials.find("mtl1")->textures());
    droplet_material.set_properties(materials.find("mtl1")->properties());

      //create ground

    materials.insert(DROPLET_HULL_MATERIAL, droplet_material);

      //scale mesh

    media::geometry::Vertex* vertex = leaf_model.mesh.vertices_data();

    for (size_t i=0, count=leaf_model.mesh.vertices_count(); i<count; i++, vertex++)
    {
      vertex->position *= LEAVES_SCALE;
    }

      //create leaves

     //add_stem(math::vec3f(0.0f, 0.0f, 0.0f), math::quatf());
    add_stem(STEAM_POSITION, to_quat(math::rotate(math::degree(90.0f), math::vec3f(0.0f, 1.0f, 0.0f))));
     //add_stem(math::vec3f(0.0f), to_quat(math::rotate(math::degree(-65.0f), math::vec3f(0.0f, 1.0f, 0.0f))));

      //configure physics

    dynamics_world->setGravity(btVector3(0, -10, 0));

    droplet_particle_shape.reset(new btSphereShape(btScalar(DROPLET_PARTICLE_RADIUS)));
    static_bind_shape.reset(new btSphereShape(btScalar(0.01f)));

    btVector3 bt_local_inertia(0, 0, 0);
    droplet_particle_shape->calculateLocalInertia(DROPLET_PARTICLE_MASS, bt_local_inertia);
    droplet_particle_local_intertia = math::vec3f(bt_local_inertia.getX(), bt_local_inertia.getY(), bt_local_inertia.getZ());

    //droplet_particle_shape->setMargin(COLLISION_MARGIN);

    setup_ground();
  }

  void setup_ground()
  {
      //graphics

    scene::Mesh::Pointer floor = scene::Mesh::create();
    media::geometry::Mesh floor_mesh = media::geometry::MeshFactory::create_box("mtl1", GROUND_SIZE, 0.01f, GROUND_SIZE);

    floor->set_mesh(floor_mesh);
    floor->bind_to_parent(*scene_root);

      //physics

    ground_shape.reset(new btBoxShape(btVector3(btScalar(GROUND_SIZE), btScalar(0.1f), btScalar(GROUND_SIZE))));

    btTransform ground_transform;
    ground_transform.setIdentity();
    ground_transform.setOrigin(btVector3(0, GROUND_OFFSET, 0));

    phys_bodies.push_back(std::make_shared<PhysBodySync>(ground_shape, 0.f, math::vec3f(0.0f), math::vec3f(0, GROUND_OFFSET, 0), math::quatf(), floor, COLLISION_GROUP_GROUND, COLLISION_MASK_GROUND, dynamics_world));
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

          std::unique_ptr<btTriangleMesh> triangle_mesh(new btTriangleMesh (true, false));

          triangle_mesh->preallocateIndices(indices.size());
          triangle_mesh->preallocateVertices(vertices.size());

          const math::vec3f* vertex = &vertices[0];
          size_t vertices_count = vertices.size();
      
          for (size_t i=0; i<vertices_count; i++, vertex++)
            triangle_mesh->findOrAddVertex(btVector3((*vertex)[0], (*vertex)[1], (*vertex)[2]), false);
          
          index = &indices[0];

          for (size_t i=0, count=indices.size(); i<count; i++, index++)
            triangle_mesh->addIndex(*index);

          triangle_mesh->getIndexedMeshArray()[0].m_numTriangles += primitive.count;

          engine_log_debug("btBvhTriangleMeshShape phys mesh shape '%s' (%u vertices, %u indices)", primitive.name.c_str(), vertices.size(), indices.size());

          shape = std::shared_ptr<btCollisionShape>(new btBvhTriangleMeshShape(triangle_mesh.release(), true, true));
          //shape = std::shared_ptr<btCollisionShape>(new btGImpactMeshShape(triangle_mesh.release()));

          //shape->setMargin(COLLISION_MARGIN);

          convex_shapes.insert(primitive.name.c_str(), shape);
        }

          //create leaf

        btVector3 bt_local_inertia(1, 1, 1);
        bt_local_inertia *= LEAF_MASS;
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
        initial_center  = rotation * initial_center + position;

        Leaf leaf;

          //configure light

        leaf.point_light = scene::PointLight::create();

        leaf.point_light->set_light_color(math::vec3f(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY)));
        leaf.point_light->set_attenuation(LIGHTS_ATTENUATION);
        leaf.point_light->set_intensity(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY));
        leaf.point_light->set_range(crand(LIGHTS_MIN_RANGE, LIGHTS_MAX_RANGE));

        //leaf.point_light->bind_to_parent(*leaf.phys_body->mesh);
        leaf.point_light->set_position(initial_center);
        leaf.point_light->bind_to_parent(*scene_root);

          //configure leaf constraint

        leaf.phys_body = phys_bodies.back();
        leaf.target_transform = leaf.phys_body->body->getWorldTransform();
        leaf.initial_center = initial_center;

        leaf.phys_body->body->setFriction(FRICTION);

        bt_local_inertia = btVector3(0, 0, 0);

        btTransform start_transform;
        start_transform.setIdentity();

        btVector3 static_body_bind_pos = btVector3(pivot_point[0], pivot_point[1], pivot_point[2]);
        start_transform.setOrigin(static_body_bind_pos);

        start_transform = leaf.phys_body->body->getWorldTransform() * start_transform;

        btRigidBody::btRigidBodyConstructionInfo rb_info(0.0f, nullptr, static_bind_shape.get(), bt_local_inertia);
        rb_info.m_startWorldTransform = start_transform;
        leaf.static_bind_body = std::make_shared<btRigidBody>(rb_info);

        dynamics_world->addRigidBody(leaf.static_bind_body.get());

        btVector3 static_bind_anchor(0, 0, 0);
        btVector3 leaf_anchor(pivot_point[0], pivot_point[1], pivot_point[2]);

        leaf.constraint = std::make_shared<btPoint2PointConstraint>(*leaf.phys_body->body, *leaf.static_bind_body,
          leaf_anchor, static_bind_anchor);

        dynamics_world->addConstraint(leaf.constraint.get(), true);

        leaves.push_back(leaf);
      }
    }
  }

  void generate_droplet()
  {
    if (!leaves.size())
      return;

    size_t leaf_index = rand() % leaves.size();

    Leaf& leaf = leaves[leaf_index];

    generate_droplet(leaf.initial_center + math::vec3f(0, 1, 0));
  }

  void generate_droplet(const math::vec3f& droplet_center)
  {
    //static float DROPLET_RADIUS = LAYERS_COUNT * DROPLET_PARTICLE_RADIUS * 3.0f;

    for (size_t i=0; i<LAYERS_COUNT; i++)
    {
      float radius = float(i+1) / LAYERS_COUNT * DROPLET_RADIUS / 2.0;

      for (size_t j=0; j<PARALLELS_COUNT; j++)
      {
        float rel_y = float(j+1) / PARALLELS_COUNT;

        rel_y = 2.0f * (rel_y - 0.5f);

        float y = rel_y * DROPLET_RADIUS;

        for (size_t k=0; k<MERIDIANS_COUNT; k++)
        {
          static float PI2 = 3.1415926f * 2.0f;

          float angle = float(k) / MERIDIANS_COUNT * PI2;

          math::vec3f position(cos(angle) * radius, y, sin(angle) * radius);

          generate_droplet_particle(position + droplet_center);
        }
      }
    }
  }

  void generate_droplet_particle(const math::vec3f& offset)
  {
    scene::Mesh::Pointer mesh = scene::Mesh::create();

    mesh->set_mesh(droplet_debug_particle_mesh);

    if (DROPLET_DEBUG_DRAW)
      mesh->bind_to_parent(*scene_root);

    phys_bodies.push_back(std::make_shared<PhysBodySync>(droplet_particle_shape, DROPLET_PARTICLE_MASS, droplet_particle_local_intertia, offset, math::quatf(), mesh, COLLISION_GROUP_DROPLET, COLLISION_MASK_DROPLET, dynamics_world));

    PhysBodySync& particle = *phys_bodies.back();

    particle.body->setFriction(FRICTION);
    particle.body->setSleepingThresholds(DROPLET_PARTICLE_LINEAR_SLEEPING_THRESHOLD, DROPLET_PARTICLE_ANGULAR_SLEEPING_THRESHOLD);
    particle.body->setAngularFactor(btVector3(0.0f, 0.0f, 0.0f));

    droplet_particles.push_back(phys_bodies.back());
  }

  void update()
  {
      //step the simulation

    dynamics_world->stepSimulation(1.f / 60.f, 10);

      //generate droplets

    if (droplet_particles.size() < MIN_PARTICLES_COUNT)
      generate_droplet();    

      //update leaves

    for (Leaf& leaf : leaves)
    {
      btRigidBody* body = leaf.phys_body->body.get();
      float inv_mass = body->getInvMass();
      float mass = inv_mass == 0.0f ? 0.0f : 1.0f / inv_mass;

      static const float TIME_STEP = 0.1f;
      static const float FORCE_FACTOR = 0.1f;
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

    droplet_particles.erase(std::remove_if(droplet_particles.begin(), droplet_particles.end(), [](const std::shared_ptr<PhysBodySync>& particle) {
      return particle->body->getWorldTransform().getOrigin().getY() < MIN_DROPLET_PARTICLE_HEIGHT;
    }), droplet_particles.end());

      //clusterize droplet particles to droplets

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      droplet->points.clear();
      droplet->hull_builder.reset();
    }

    for (std::shared_ptr<PhysBodySync>& particle : droplet_particles)
    {
      btVector3 bt_position = particle->body->getWorldTransform().getOrigin();
      math::vec3f position(bt_position.getX(), bt_position.getY(), bt_position.getZ());
      bool added = false;

      for (std::shared_ptr<Droplet>& droplet : droplets)
      {
        float distance = length(droplet->center - position);

        static const float CLUSTER_RADIUS = DROPLET_RADIUS * 2.5f;

        if (distance < CLUSTER_RADIUS)
        {
          droplet->points.push_back(position);
          droplet->bodies.push_back(particle);
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

        droplet->hull_mesh = scene::Mesh::create();

        droplet->hull_mesh->set_environment_map_required(true); //require envmap prerendering

        droplet->hull_mesh->set_mesh(droplet->hull_builder.mesh());
        //droplet->hull_mesh->set_mesh(media::geometry::MeshFactory::create_box(DROPLET_HULL_MATERIAL, 1, 1, 1));        

        if (!DROPLET_DEBUG_DRAW)
          droplet->hull_mesh->bind_to_parent(*scene_root);
        
        droplets.push_back(droplet);
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

        static float TARGET_DISTANCE = 15.0f;
        static float POSITION_INTERPOLATION_FACTOR = 0.001f;
        static float TARGET_INTERPOLATION_FACTOR = 0.001f;

        math::vec3f required_camera_position = center + normalize(math::vec3f(1, 0.5, 0)) * TARGET_DISTANCE;
        math::vec3f required_camera_target = center;
        math::vec3f current_camera_position = math::vec3f(camera->world_tm() * math::vec4f(0, 0, 0, 1));
        math::vec3f current_camera_target = current_camera_position + math::vec3f(camera->world_tm() * math::vec4f(0, 1, 0, 0));
        math::vec3f new_camera_position = current_camera_position + (required_camera_position - current_camera_position) * POSITION_INTERPOLATION_FACTOR;
        math::vec3f new_camera_target = current_camera_target + (required_camera_target - current_camera_target) * TARGET_INTERPOLATION_FACTOR;

        //camera->set_position(inverse(camera->parent()->world_tm()) * new_camera_position);
        //camera->world_look_to(new_camera_target, math::vec3f(0, 1, 0));
      }
    }

    //configure droplets

    //engine_log_debug("Droplets count: %d (particles count %d)", droplets.size(), droplet_particles.size());

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

      math::vec3f sigma;

      for (const math::vec3f& point : droplet->points)
      {
        sigma += (point - droplet->center) * (point - droplet->center);
      }

      sigma = sqrt(length(sigma / droplet->points.size()));

      for (const math::vec3f& point : droplet->points)
      {
        if (length(point - droplet->center) > length(sigma) * 2.0f)
          continue;
        droplet->hull_builder.add_point(point);
      }

            ///TODO test only!!!!!

        //droplet->hull_mesh->set_position(droplet->center);
      //engine_log_debug("Droplet center: %f %f %f", droplet->center[0], droplet->center[1], droplet->center[2]);
    }

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      if (!DROPLET_DEBUG_DRAW)
        droplet->hull_mesh->set_environment_map_local_point(inverse(droplet->hull_mesh->world_tm()) * droplet->center);

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
        droplet->remove_counter++;
      else
        droplet->remove_counter = 0;
    }

    droplets.erase(std::remove_if(droplets.begin(), droplets.end(), [](const std::shared_ptr<Droplet>& droplet) { return droplet->remove_counter > DROPLET_REMOVE_COUNTER_THRESHOLD; }),
      droplets.end());

    //build droplet hulls

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      droplet->hull_builder.build_hull(DROPLET_HULL_MATERIAL);
    }

    //apply forces to droplets

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      for (std::shared_ptr<PhysBodySync>& particle : droplet->bodies)
      {
        btVector3 bt_position = particle->body->getWorldTransform().getOrigin();
        btVector3 bt_velocity = particle->body->getLinearVelocity();
        math::vec3f position(bt_position.getX(), bt_position.getY(), bt_position.getZ());
        math::vec3f velocity(bt_velocity.getX(), bt_velocity.getY(), bt_velocity.getZ());

        static const float FORCE_DISTANCE = DROPLET_RADIUS * 2.0f;

        static const float DROPLET_FORCE = 0.0002f;
        static const float EPSILON = DROPLET_PARTICLE_RADIUS * 2.0f;
        static const float TIME_STEP = 1.0f / 60.0f;

        math::vec3f force = droplet->center - position;// - velocity * TIME_STEP;// + math::vec3f(0, 0.1f * DROPLET_PARTICLE_RADIUS, 0);
        float distance = length(force);

        if (distance < FORCE_DISTANCE && distance > EPSILON)
        {
          //force = normalize(force) * (DROPLET_RADIUS - distance) / DROPLET_RADIUS * DROPLET_FORCE;
          //force = normalize(force) * (FORCE_DISTANCE - distance) * DROPLET_FORCE;
          force *= DROPLET_FORCE;
          particle->body->applyCentralForce(btVector3(force[0], force[1], force[2]));
          //particle->body->applyCentralForce(btVector3(0, -10, 0));
          //particle1->body->applyCentralForce(-btVector3(force[0], force[1], force[2]));
        }

        //static const float VELOCITY_BRAKE_FACTOR = 0.0001f;

        //math::vec3f velocity_brake = -velocity * VELOCITY_BRAKE_FACTOR;

        //particle->body->applyCentralForce(btVector3(velocity_brake[0], velocity_brake[1], velocity_brake[2]));

        /*/ math::vec3f dir = droplet->center - position;

        if (length(dir) < FORCE_DISTANCE)
        {
          static const float DROPLET_FORCE = .003f;
          //math::vec3f force = position * DROPLET_FORCE / particle->body->getInvMass() / TIME_STEP;
          math::vec3f force = dir * DROPLET_FORCE;

          particle->body->applyCentralForce(btVector3(force[0], force[1], force[2]));
        }*/
      }
    }

      //sync bodies with scene

    for (std::shared_ptr<PhysBodySync>& particle : phys_bodies)
    {
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

void World::update()
{
  impl->update();
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
