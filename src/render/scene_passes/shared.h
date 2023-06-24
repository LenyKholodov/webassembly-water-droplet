#include <render/scene_render.h>

#include <scene/camera.h>
#include <scene/mesh.h>
#include <scene/light.h>
#include <scene/projectile.h>

#include <application/window.h>

#include <common/exception.h>
#include <common/log.h>
#include <common/string.h>
#include <common/component.h>

#include <math/utility.h>

namespace engine {
namespace render {
namespace scene {
namespace passes {

typedef std::vector<engine::scene::Entity::Pointer> EntityArray;
typedef std::vector<engine::scene::Mesh::Pointer> MeshArray;
typedef std::vector<engine::scene::PointLight::Pointer> PointLightArray;
typedef std::vector<engine::scene::SpotLight::Pointer> SpotLightArray;
typedef std::vector<engine::scene::Projectile::Pointer> ProjectileArray;

/// Rendering mesh data
struct RenderableMesh
{
  low_level::Mesh mesh;
  size_t vertices_count;
  size_t indices_count;

  RenderableMesh(media::geometry::Mesh& mesh, ScenePassContext& context)
    : mesh(context.device().create_mesh(mesh, context.materials()))
    , vertices_count(mesh.vertices_count())
    , indices_count(mesh.indices_count())
  {
  }

  static RenderableMesh* get(media::geometry::Mesh& mesh, ScenePassContext& context)
  {
    RenderableMesh* renderable_mesh = mesh.find_user_data<RenderableMesh>();

    if (!renderable_mesh)
    {
      renderable_mesh = &mesh.set_user_data(RenderableMesh(mesh, context));
    }

    renderable_mesh->mesh.update_geometry(mesh);

    return renderable_mesh;
  }
};

/// Shadow
struct Shadow
{
  low_level::Texture shadow_texture;
  low_level::Pass shadow_pass;
  low_level::FrameBuffer shadow_frame_buffer;
  FrameNode shadow_frame;
  math::mat4f shadow_tm;

  Shadow(engine::render::low_level::Device& device, const low_level::Program& program, size_t shadow_map_size)
    : shadow_texture(device.create_texture2d(shadow_map_size, shadow_map_size, low_level::PixelFormat_D24, 1))
    , shadow_pass(device.create_pass(program))
    , shadow_frame_buffer(device.create_frame_buffer())
    , shadow_tm(1.0f)
  {
    shadow_texture.set_min_filter(low_level::TextureFilter_Point);

    shadow_frame_buffer.attach_depth_buffer(shadow_texture);
    shadow_frame_buffer.set_viewport(low_level::Viewport(0, 0, (int)shadow_map_size, (int)shadow_map_size));

    shadow_pass.set_frame_buffer(shadow_frame_buffer);
    shadow_pass.set_depth_stencil_state(low_level::DepthStencilState(true, true, low_level::CompareMode_Less));
  }
};

/// Portal
struct Portal
{
  low_level::Texture texture;
  low_level::RenderBuffer depth_render_buffer;
  low_level::FrameBuffer frame_buffer;

  Portal(
    engine::render::low_level::Device& device,
    const low_level::Texture& texture,
    size_t layer,
    const low_level::RenderBuffer& depth_render_buffer)
    : texture(texture)
    , depth_render_buffer(depth_render_buffer)
    , frame_buffer(device.create_frame_buffer())
  {
    frame_buffer.attach_color_target(texture, layer, 0);
    frame_buffer.attach_depth_buffer(depth_render_buffer);
  }
};

/// Environment map
struct EnvironmentMap
{
  low_level::Texture portal_texture;
  low_level::RenderBuffer depth_render_buffer;
  std::vector<std::shared_ptr<Portal>> portals;
  low_level::TextureList textures;

  EnvironmentMap(
    engine::render::low_level::Device& device,
    size_t portal_texture_size)
    : portal_texture(device.create_texture_cubemap(portal_texture_size, portal_texture_size, low_level::PixelFormat_RGBA8, 1))
    , depth_render_buffer(device.create_render_buffer(portal_texture_size, portal_texture_size, low_level::PixelFormat_D16))
  {
    portal_texture.set_min_filter(low_level::TextureFilter_Linear);

    for (size_t i = 0; i < 6; ++i)
    {
      portals.push_back(std::make_shared<Portal>(device, portal_texture, i, depth_render_buffer));
    }

    textures.insert("environmentMap", portal_texture);
  }

  static EnvironmentMap* find(engine::scene::Entity& entity)
  {
    return entity.find_user_data<EnvironmentMap>();
  }

  static EnvironmentMap* get(engine::scene::Entity& entity, ScenePassContext& context, size_t texture_size)
  {
    EnvironmentMap* environment_map = entity.find_user_data<EnvironmentMap>();

    if (!environment_map)
    {
      environment_map = &entity.set_user_data(EnvironmentMap(context.device(), texture_size));
    }

    return environment_map;
  }
};

/// Projectile render data
struct RenderableProjectile
{
  low_level::Texture texture;
  low_level::Material material;
  low_level::Primitive plane;
  common::PropertyMap properties;

  RenderableProjectile(const char* image_name, const low_level::Texture& shadow_texture, engine::render::low_level::Device& device)
    : texture(device.create_texture2d(image_name))
    , plane(device.create_plane(material))
  {
    texture.generate_mips();
    texture.set_min_filter(low_level::TextureFilter_LinearMipLinear);
    texture.set_mag_filter(low_level::TextureFilter_Linear);

    low_level::TextureList textures = material.textures();

    textures.insert("projectileTexture", texture);
    textures.insert("shadowTexture", shadow_texture);

    float tex_size_step = 1.0f / shadow_texture.width();

    properties.set("shadowMapPixelSize", math::vec2f(tex_size_step));
  }
};

/// Scene visitor
class SceneVisitor : private engine::scene::ISceneVisitor
{
  public:
    /// Constructor
    SceneVisitor();

    /// Meshes
    const MeshArray& meshes() const;

    /// Point lights
    const PointLightArray& point_lights() const;

    /// Spot lights
    const SpotLightArray& spot_lights() const;

    /// Projectiles
    const ProjectileArray& projectiles() const;

    /// Nodes with prerenderings
    const EntityArray& prerender_entities() const;

    /// Reset results
    void reset();

    /// Traverse scene
    void traverse(engine::scene::Node&, const ScenePassOptions* options = nullptr);

  private:
    void visit(engine::scene::Mesh&) override;
    void visit(engine::scene::Entity&) override;
    void visit(engine::scene::SpotLight&) override;
    void visit(engine::scene::PointLight&) override;
    void visit(engine::scene::Projectile&) override;

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}}}
