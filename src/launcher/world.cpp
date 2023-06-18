#include "shared.h"

#include <common/log.h>

#include "btBulletDynamicsCommon.h"

using namespace engine::common;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::scene;
using namespace engine;

const char* LEAF_MESH = "media/meshes/leaf.obj";
const float DROPLET_PARTICLE_RADIUS = 0.1f;
const float DROPLET_PARTICLE_MASS = 0.1f;
const float DROPLET_GENERATION_RADIUS = DROPLET_PARTICLE_RADIUS * 5.0f;
const size_t DROPLET_GENERATION_PARTICLES_COUNT = 30;
const float GROUND_SIZE = 50.0f;
const float GROUND_OFFSET = 0;

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

struct DropletParticle
{
  std::shared_ptr<btRigidBody> body;
  scene::Mesh::Pointer mesh;

  DropletParticle(const std::shared_ptr<btRigidBody>& body, const scene::Mesh::Pointer& mesh)
    : body(body)
    , mesh(mesh)
  {
  }
};

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
  std::shared_ptr<btRigidBody> ground_body;
  std::vector<std::shared_ptr<DropletParticle>> droplet_particles;
  media::geometry::Mesh droplet_debug_particle_mesh;

  Impl(scene::Node::Pointer scene_root, SceneRenderer& scene_renderer)
    : leaf_model(media::geometry::MeshFactory::load_obj_model(LEAF_MESH))
    , scene_root(scene_root)
    , collision_configuration(new btDefaultCollisionConfiguration())
    , dispatcher(new btCollisionDispatcher(collision_configuration.get()))
    , broadphase(new btDbvtBroadphase())
    , solver(new btSequentialImpulseConstraintSolver())
    , dynamics_world(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collision_configuration.get()))
    , droplet_debug_particle_mesh(media::geometry::MeshFactory::create_sphere("mtl1", 1.0f))
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

    scene::Mesh::Pointer mesh = scene::Mesh::create();

    mesh->set_mesh(leaf_model.mesh);
    mesh->set_scale(math::vec3f(0.5f));
    mesh->bind_to_parent(*scene_root);

      //configure physics

    dynamics_world->setGravity(btVector3(0, -10, 0));

    droplet_particle_shape.reset(new btSphereShape(btScalar(DROPLET_PARTICLE_RADIUS)));

    setup_ground();

    //for test only

    generate_droplet();
  }

  void setup_ground()
  {
    ground_shape.reset(new btBoxShape(btVector3(btScalar(GROUND_SIZE), btScalar(0.1f), btScalar(GROUND_SIZE))));

    btTransform ground_transform;
    ground_transform.setIdentity();
    ground_transform.setOrigin(btVector3(0, GROUND_OFFSET, 0));

    btScalar mass(0.);

    //rigidbody is dynamic if and only if mass is non zero, otherwise static
    bool is_dynamic = (mass != 0.f);

    btVector3 local_inertia(0, 0, 0);
    if (is_dynamic)
        ground_shape->calculateLocalInertia(mass, local_inertia);

    //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* my_motion_state = new btDefaultMotionState(ground_transform);
    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, my_motion_state, ground_shape.get(), local_inertia);

    ground_body.reset(new btRigidBody(rb_info));

    //add the body to the dynamics world
    dynamics_world->addRigidBody(ground_body.get());

    scene::Mesh::Pointer floor = scene::Mesh::create();
    media::geometry::Mesh floor_mesh = media::geometry::MeshFactory::create_box("mtl1", GROUND_SIZE, 0.01f, GROUND_SIZE);

    floor->set_mesh(floor_mesh);
    floor->set_position(math::vec3f(ground_transform.getOrigin().getX(), ground_transform.getOrigin().getY(), ground_transform.getOrigin().getZ()));
    floor->bind_to_parent(*scene_root);
  }

  void generate_droplet()
  {
    for (size_t i=0; i<DROPLET_GENERATION_PARTICLES_COUNT; i++)
    {
      btVector3 offset(0, 10.0f + crand() * DROPLET_GENERATION_RADIUS, 0);

      generate_droplet_particle(offset);
    }
  }

  void generate_droplet_particle(const btVector3& offset)
  {
    /// Create Dynamic Objects
    btTransform start_transform;
    start_transform.setIdentity();

    btScalar mass(DROPLET_PARTICLE_MASS);

    //rigidbody is dynamic if and only if mass is non zero, otherwise static
    bool isDynamic = (mass != 0.f);

    btVector3 local_inertia(0, 0, 0);
    if (isDynamic)
        droplet_particle_shape->calculateLocalInertia(mass, local_inertia); //TODO: optimize, out

    start_transform.setOrigin(offset);

    //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
    btDefaultMotionState* my_motion_state = new btDefaultMotionState(start_transform);
    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, my_motion_state, droplet_particle_shape.get(), local_inertia);
    std::shared_ptr<btRigidBody> body(new btRigidBody(rb_info));

    dynamics_world->addRigidBody(body.get());

    scene::Mesh::Pointer mesh = scene::Mesh::create();

    mesh->set_mesh(droplet_debug_particle_mesh);
    mesh->set_scale(math::vec3f(DROPLET_PARTICLE_RADIUS));
    mesh->bind_to_parent(*scene_root);

    droplet_particles.push_back(std::make_shared<DropletParticle>(body, mesh));
  }

  void update()
  {
      //step the simulation

    dynamics_world->stepSimulation(1.f / 60.f, 10);

      //sync debug droplet particles

    for (std::shared_ptr<DropletParticle>& particle : droplet_particles)
    {
      btTransform transform;
      particle->body->getMotionState()->getWorldTransform(transform);
      particle->mesh->set_position(math::vec3f(transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ()));
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
