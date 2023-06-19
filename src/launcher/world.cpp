#include "shared.h"

#include <common/log.h>
#include <common/named_dictionary.h>
#include <math/utility.h>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"

using namespace engine::common;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::scene;
using namespace engine;

const char* LEAF_MESH = "media/meshes/leaf.obj";
const float DROPLET_PARTICLE_RADIUS = 0.1f;
const float DROPLET_PARTICLE_MASS = 0.1f;
const float DROPLET_GENERATION_RADIUS = DROPLET_PARTICLE_RADIUS * 10.0f;
const size_t DROPLET_GENERATION_PARTICLES_COUNT = 30;
const float GROUND_SIZE = 50.0f;
const float GROUND_OFFSET = -70.f;
const float LEAF_MASS = 1.0f;

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

    dynamics_world->addRigidBody(body.get());
  }  
};

struct Leaf
{
  std::shared_ptr<PhysBodySync> phys_body;
  std::shared_ptr<btRigidBody> static_bind_body;
  std::shared_ptr<btTypedConstraint> constraint;
  btTransform target_transform;
};

struct Droplet
{
  math::vec3f center;
  std::vector<math::vec3f> points;
  std::vector<std::shared_ptr<PhysBodySync>> bodies;
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

  Impl(scene::Node::Pointer scene_root, SceneRenderer& scene_renderer)
    : leaf_model(media::geometry::MeshFactory::load_obj_model(LEAF_MESH))
    , scene_root(scene_root)
    , collision_configuration(new btDefaultCollisionConfiguration())
    , dispatcher(new btCollisionDispatcher(collision_configuration.get()))
    , broadphase(new btDbvtBroadphase())
    , solver(new btSequentialImpulseConstraintSolver())
    , dynamics_world(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collision_configuration.get()))
    , droplet_debug_particle_mesh(media::geometry::MeshFactory::create_sphere("mtl1", DROPLET_PARTICLE_RADIUS))
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

      //create leaves

     //add_stem(math::vec3f(0.0f, 0.0f, 0.0f), math::quatf());
     add_stem(math::vec3f(-30.0f, 0.0f, 30.0f), to_quat(math::rotate(math::degree(65.0f), math::vec3f(0.0f, 1.0f, 0.0f))));
     //add_stem(math::vec3f(0.0f), to_quat(math::rotate(math::degree(-65.0f), math::vec3f(0.0f, 1.0f, 0.0f))));

      //configure physics

    dynamics_world->setGravity(btVector3(0, -10, 0));

    droplet_particle_shape.reset(new btSphereShape(btScalar(DROPLET_PARTICLE_RADIUS)));
    static_bind_shape.reset(new btSphereShape(btScalar(0.01f)));

    btVector3 bt_local_inertia(0, 0, 0);
    droplet_particle_shape->calculateLocalInertia(DROPLET_PARTICLE_MASS, bt_local_inertia);
    droplet_particle_local_intertia = math::vec3f(bt_local_inertia.getX(), bt_local_inertia.getY(), bt_local_inertia.getZ());

    setup_ground();

    //for test only

    generate_droplet();
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

    phys_bodies.push_back(std::make_shared<PhysBodySync>(ground_shape, 0.f, math::vec3f(0.0f), math::vec3f(0, GROUND_OFFSET, 0), math::quatf(), floor, dynamics_world));
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

          convex_shapes.insert(primitive.name.c_str(), shape);
        }

          //create leaf

        btVector3 bt_local_inertia(1, 1, 1);
        bt_local_inertia *= LEAF_MASS;
        math::vec3f local_inertia(bt_local_inertia.getX(), bt_local_inertia.getY(), bt_local_inertia.getZ());

        phys_bodies.push_back(std::make_shared<PhysBodySync>(shape, LEAF_MASS, local_inertia, position, rotation, mesh, dynamics_world));

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

          //configure leaf constraint

        Leaf leaf;

        leaf.phys_body = phys_bodies.back();
        leaf.target_transform = leaf.phys_body->body->getWorldTransform();

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
    for (size_t i=0; i<DROPLET_GENERATION_PARTICLES_COUNT; i++)
    {
      math::vec3f offset(0, 10.0f + crand() * DROPLET_GENERATION_RADIUS, 0);

      generate_droplet_particle(offset);
    }
  }

  void generate_droplet_particle(const math::vec3f& offset)
  {
    scene::Mesh::Pointer mesh = scene::Mesh::create();

    mesh->set_mesh(droplet_debug_particle_mesh);
    mesh->bind_to_parent(*scene_root);

    phys_bodies.push_back(std::make_shared<PhysBodySync>(droplet_particle_shape, DROPLET_PARTICLE_MASS, droplet_particle_local_intertia, offset, math::quatf(), mesh, dynamics_world));

    droplet_particles.push_back(phys_bodies.back());
  }

  void update()
  {
      //step the simulation

    dynamics_world->stepSimulation(1.f / 60.f, 10);

      //update leaves

    for (Leaf& leaf : leaves)
    {
      btRigidBody* body = leaf.phys_body->body.get();
      float inv_mass = body->getInvMass();
      float mass = inv_mass == 0.0f ? 0.0f : 1.0f / inv_mass;

      static const float TIME_STEP = 0.01f;
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

      //update droplets

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      droplet->points.clear();
    }

    for (std::shared_ptr<PhysBodySync>& particle : droplet_particles)
    {
      btVector3 bt_position = particle->body->getWorldTransform().getOrigin();
      math::vec3f position(bt_position.getX(), bt_position.getY(), bt_position.getZ());
      bool added = false;

      for (std::shared_ptr<Droplet>& droplet : droplets)
      {
        float distance = length(droplet->center - position);

        static const float DROPLET_RADIUS = 2.f;

        if (distance < DROPLET_RADIUS)
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

        droplets.push_back(droplet);
      }
    }

    engine_log_debug("Droplets count: %d", droplets.size());

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      math::vec3f center(0.0f);

      if (droplet->points.empty())
      {
        droplet->center = center;
        continue;
      }
      
      for (const math::vec3f& point : droplet->points)
        center += point;
      
      droplet->center = center / droplet->points.size();

      engine_log_debug("Droplet center: %f %f %f", droplet->center[0], droplet->center[1], droplet->center[2]);
    }

    droplets.erase(std::remove_if(droplets.begin(), droplets.end(), [](const std::shared_ptr<Droplet>& droplet) { return droplet->points.empty(); }),
      droplets.end());

    for (std::shared_ptr<Droplet>& droplet : droplets)
    {
      for (std::shared_ptr<PhysBodySync>& particle : droplet->bodies)
      {
        btVector3 bt_position = particle->body->getWorldTransform().getOrigin();
        math::vec3f position(bt_position.getX(), bt_position.getY(), bt_position.getZ());
        math::vec3f velocity(particle->body->getLinearVelocity().getX(), particle->body->getLinearVelocity().getY(), particle->body->getLinearVelocity().getZ());

        static const float TIME_STEP = 0.01f;
        static const float DROPLET_RADIUS = 1.5f;
        static const float DROPLET_FORCE = 0.04f;
        static const float EPSILON = 0.001f;

        math::vec3f force = droplet->center - (position + velocity * TIME_STEP);
        float distance = length(force);

        if (distance < DROPLET_RADIUS && distance > EPSILON)
        {
          force = normalize(force) * (DROPLET_RADIUS - distance) / DROPLET_RADIUS * DROPLET_FORCE;
          particle->body->applyCentralForce(btVector3(force[0], force[1], force[2]));
        }
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
};

World::World(scene::Node::Pointer scene_root, SceneRenderer& scene_render)
  : impl(new Impl(scene_root, scene_render))
{

}

World::~World()
{
}

void World::update()
{
  impl->update();
}
