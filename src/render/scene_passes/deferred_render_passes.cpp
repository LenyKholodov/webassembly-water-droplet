#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

namespace engine {
namespace render {
namespace scene {
namespace passes {

///
/// Constants
///

static const char* GBUFFER_PROGRAM_FILE = "media/shaders/phong_gbuffer.glsl";
static const char* DEFERRED_LIGHTING_PROGRAM_FILE = "media/shaders/lighting.glsl";

///
/// G-Buffer
///

struct GBufferPass : IScenePass
{
  public:
    GBufferPass(SceneRenderer& renderer, Device& device)
      : g_buffer_width(device.window().frame_buffer_width())
      , g_buffer_height(device.window().frame_buffer_height())
      , g_buffer_program(device.create_program_from_file(GBUFFER_PROGRAM_FILE))
      , g_buffer_pass(device.create_pass(g_buffer_program))
      , shared_textures(renderer.textures())
      , shared_frames(renderer.frame_nodes())
      , positions_texture(device.create_texture2d(g_buffer_width, g_buffer_height, PixelFormat_RGB16F, 1))
      , normals_texture(device.create_texture2d(g_buffer_width, g_buffer_height, PixelFormat_RGB16F, 1))
      , albedo_texture(device.create_texture2d(g_buffer_width, g_buffer_height, PixelFormat_RGBA8, 1))
      , specular_texture(device.create_texture2d(g_buffer_width, g_buffer_height, PixelFormat_RGBA8, 1))
      , g_buffer_depth(device.create_render_buffer(g_buffer_width, g_buffer_height, PixelFormat_D24))
      , g_buffer_frame_buffer(device.create_frame_buffer())
    {
      engine_log_debug("Creating G-Buffer...");

      shared_frames.insert("g_buffer", frame);

      shared_textures.insert("positionTexture", positions_texture);
      shared_textures.insert("normalTexture", normals_texture);
      shared_textures.insert("albedoTexture", albedo_texture);
      shared_textures.insert("specularTexture", specular_texture);

      positions_texture.set_min_filter(TextureFilter_Point);
      normals_texture.set_min_filter(TextureFilter_Point);
      albedo_texture.set_min_filter(TextureFilter_Point);
      specular_texture.set_min_filter(TextureFilter_Point);

      g_buffer_frame_buffer.attach_color_target(positions_texture);
      g_buffer_frame_buffer.attach_color_target(normals_texture);
      g_buffer_frame_buffer.attach_color_target(albedo_texture);
      g_buffer_frame_buffer.attach_color_target(specular_texture);
      g_buffer_frame_buffer.attach_depth_buffer(g_buffer_depth);

      g_buffer_frame_buffer.reset_viewport();

      g_buffer_pass.set_frame_buffer(g_buffer_frame_buffer);
      g_buffer_pass.set_clear_color(0.0f);
      g_buffer_pass.set_depth_stencil_state(DepthStencilState(true, true, CompareMode_Less));

      engine_log_debug("G-Buffer has been created: %ux%u", g_buffer_width, g_buffer_height);
    }

    ~GBufferPass()
    {
      shared_textures.remove("positionTexture");
      shared_textures.remove("normalTexture");
      shared_textures.remove("albedoTexture");
      shared_textures.remove("specularTexture");

      shared_frames.remove("g_buffer");
    }

    static IScenePass* create(SceneRenderer& renderer, Device& device)
    {
      return new GBufferPass(renderer, device);
    }

    void get_dependencies(std::vector<std::string>&)
    {
    }

    void prerender(ScenePassContext& context)
    {
    }    

    void render(ScenePassContext& context)
    {
      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

        //traverse scene

      visitor.traverse(*root_node, &context.options());

        //draw geometry

      for (auto& mesh : visitor.meshes())
      {
        render_mesh(*mesh, context);
      }

        //clear data

      visitor.reset();

        //update frame

      frame.add_pass(g_buffer_pass);
      context.root_frame_node().add_dependency(frame);
    }

  private:
    void render_mesh(engine::scene::Mesh& mesh, ScenePassContext& context)
    {
        //create mesh data

      RenderableMesh* renderable_mesh = RenderableMesh::get(mesh.mesh(), context);

        //add mesh to pass

      g_buffer_pass.add_mesh(renderable_mesh->mesh, mesh.world_tm(), mesh.first_primitive(), mesh.primitives_count());
    }

  private:
    size_t g_buffer_width;
    size_t g_buffer_height;
    Program g_buffer_program;
    Pass g_buffer_pass;
    TextureList shared_textures;
    FrameNodeList shared_frames;
    Texture positions_texture;
    Texture normals_texture;
    Texture albedo_texture;
    Texture specular_texture;
    RenderBuffer g_buffer_depth;
    FrameBuffer g_buffer_frame_buffer;
    SceneVisitor visitor;
    FrameNode frame;
};

///
/// Deferred lighting pass
///

struct DeferredLightingPass : IScenePass
{
  public:
    DeferredLightingPass(SceneRenderer& renderer, Device& device)
      : deferred_lighting_program(device.create_program_from_file(DEFERRED_LIGHTING_PROGRAM_FILE))
      , deferred_lighting_pass(device.create_pass(deferred_lighting_program))
      , plane(device.create_plane(Material()))
    {
      deferred_lighting_pass.set_depth_stencil_state(DepthStencilState(false, false, CompareMode_AlwaysPass));

      engine_log_debug("Deferred Lighting pass has been created");
    }

    static IScenePass* create(SceneRenderer& renderer, Device& device)
    {
      return new DeferredLightingPass(renderer, device);
    }

    void get_dependencies(std::vector<std::string>& deps)
    {
      deps.push_back("Shadow Maps Rendering");
      deps.push_back("G-Buffer");
      deps.push_back("Projectile Maps Rendering");
    }

    void prerender(ScenePassContext& context)
    {
    }    

    void render(ScenePassContext& context)
    {
        //search for G-Buffer frame and add it to dependency list

      if (!g_buffer_frame_initialized)
      {
        g_buffer_frame = context.frame_nodes().get("g_buffer");
        g_buffer_frame_initialized = true;
      }

      frame.add_dependency(g_buffer_frame);

        //traverse scene

      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

        //configure framebuffer

      deferred_lighting_pass.set_frame_buffer(context.default_frame_buffer());
      deferred_lighting_pass.set_clear_color(context.clear_color());

        //traverse scene

      visitor.traverse(*root_node, &context.options());

        //configure params

      setup_point_lights(visitor.point_lights(), context);
      setup_spot_lights(visitor.spot_lights(), context);

        //add plane to deferred lightins

      deferred_lighting_pass.add_primitive(plane);

        //add lighting pass to frame

      frame.add_pass(deferred_lighting_pass);

        //add this frame to root frame

      context.root_frame_node().add_dependency(frame);

        //clear data

      visitor.reset();
      point_light_positions.clear();
      point_light_colors.clear();
      point_light_attenuations.clear();
      point_light_ranges.clear();
      spot_light_positions.clear();
      spot_light_directions.clear();
      spot_light_colors.clear();
      spot_light_attenuations.clear();
      spot_light_ranges.clear();
      spot_light_angles.clear();
      spot_light_exponents.clear();
      spot_lights_shadow_matrices.clear();
    }

  private:
    void setup_point_lights(const PointLightArray& lights, ScenePassContext& context)
    {
        //setup lights

      common::PropertyMap properties = frame.properties();
      
      static constexpr size_t MAX_LIGHTS_COUNT = 32; //TODO: batch light rendering in several passes

      point_light_positions.reserve(MAX_LIGHTS_COUNT);
      point_light_colors.reserve(MAX_LIGHTS_COUNT);
      point_light_attenuations.reserve(MAX_LIGHTS_COUNT);
      point_light_ranges.reserve(MAX_LIGHTS_COUNT);

      size_t lights_count = lights.size();

      engine_check(lights_count <= MAX_LIGHTS_COUNT);

      for (auto& light : lights)
      {
        float intensity = light->intensity();

        if (intensity < 0)
          intensity = 0;

        math::vec3f position = light->world_tm() * math::vec3f(0, 0, 0, 1.0f);
        math::vec3f color = light->light_color() * intensity;
        math::vec3f attenuation = light->attenuation();
        float range = light->range();

        point_light_positions.push_back(position);
        point_light_colors.push_back(color);
        point_light_attenuations.push_back(attenuation);
        point_light_ranges.push_back(range);
      }

      for (size_t i=lights.size(); i<MAX_LIGHTS_COUNT; i++)
      {
        point_light_positions.push_back(0.0f);
        point_light_colors.push_back(0.0f);
        point_light_attenuations.push_back(math::vec3f(1.0f));
        point_light_ranges.push_back(0.0f);
      }

        //bind properties

      properties.set("pointLightPositions", point_light_positions);
      properties.set("pointLightColors", point_light_colors);
      properties.set("pointLightAttenuations", point_light_attenuations);
      properties.set("pointLightRanges", point_light_ranges);
    }

    void setup_spot_lights(const SpotLightArray& lights, ScenePassContext& context)
    {
        //setup lights

      common::PropertyMap properties = frame.properties();
      
      static constexpr size_t MAX_LIGHTS_COUNT = 2; //TODO: batch light rendering in several passes

      spot_light_positions.reserve(MAX_LIGHTS_COUNT);
      spot_light_directions.reserve(MAX_LIGHTS_COUNT);
      spot_light_colors.reserve(MAX_LIGHTS_COUNT);
      spot_light_attenuations.reserve(MAX_LIGHTS_COUNT);
      spot_light_ranges.reserve(MAX_LIGHTS_COUNT);
      spot_light_angles.reserve(MAX_LIGHTS_COUNT);
      spot_light_exponents.reserve(MAX_LIGHTS_COUNT);

      size_t lights_count = lights.size();

      engine_check(lights_count <= MAX_LIGHTS_COUNT);

      for (auto& light : lights)
      {
        float intensity = light->intensity();

        if (intensity < 0)
          intensity = 0;

        math::vec3f position = light->world_tm() * math::vec3f(0, 0, 0, 1.0f);
        math::vec3f direction = normalize(math::vec3f(light->world_tm() * math::vec4f(0, 0, 1.0f, 0)));
        math::vec3f color = light->light_color() * intensity;
        math::vec3f attenuation = light->attenuation();
        float range = light->range();
        float angle = math::radian(light->angle()) / 2;
        float exponent = light->exponent();
        Shadow* shadow = light->find_user_data<Shadow>();
        const math::mat4f& shadow_tm = shadow->shadow_tm;

        engine_check(shadow);
        
          //TODO: texture arrays binding to shader program
        deferred_lighting_pass.textures().remove("shadowTexture");
        deferred_lighting_pass.textures().insert("shadowTexture", shadow->shadow_texture);

        float tex_size_step = 1.0f / shadow->shadow_texture.width();

        deferred_lighting_pass.properties().set("shadowMapPixelSize", math::vec2f(tex_size_step));

        frame.add_dependency(shadow->shadow_frame);
        
        spot_light_positions.push_back(position);
        spot_light_directions.push_back(direction);
        spot_light_colors.push_back(color);
        spot_light_attenuations.push_back(attenuation);
        spot_light_ranges.push_back(range);
        spot_light_angles.push_back(angle);
        spot_light_exponents.push_back(exponent);
        spot_lights_shadow_matrices.push_back(shadow_tm);
      }

      for (size_t i=lights.size(); i<MAX_LIGHTS_COUNT; i++)
      {
        spot_light_positions.push_back(0.0f);
        spot_light_directions.push_back(math::vec3f(0, 0, 1.0f));
        spot_light_colors.push_back(0.0f);
        spot_light_attenuations.push_back(math::vec3f(1.0f));
        spot_light_ranges.push_back(0.f);
        spot_light_angles.push_back(0);
        spot_light_exponents.push_back(1.0f);
        spot_lights_shadow_matrices.push_back(0.0f);
      }

        //bind properties

      properties.set("spotLightPositions", spot_light_positions);
      properties.set("spotLightDirections", spot_light_directions);
      properties.set("spotLightColors", spot_light_colors);
      properties.set("spotLightAttenuations", spot_light_attenuations);
      properties.set("spotLightRanges", spot_light_ranges);
      properties.set("spotLightAngles", spot_light_angles);
      properties.set("spotLightExponents", spot_light_exponents);
      properties.set("spotLightShadowMatrices", spot_lights_shadow_matrices);
    }

  private:
    typedef std::vector<math::mat4f> Mat4fArray;
    typedef std::vector<math::vec3f> Vec3fArray;
    typedef std::vector<float> FloatArray;

  private:
    Program deferred_lighting_program;
    Pass deferred_lighting_pass;
    Primitive plane;
    FrameNode frame;    
    FrameNode g_buffer_frame;
    bool g_buffer_frame_initialized = false;
    SceneVisitor visitor;
    Vec3fArray point_light_positions;
    Vec3fArray point_light_colors;
    Vec3fArray point_light_attenuations;
    FloatArray point_light_ranges;
    Vec3fArray spot_light_positions;
    Vec3fArray spot_light_directions;
    Vec3fArray spot_light_colors;
    Vec3fArray spot_light_attenuations;
    FloatArray spot_light_ranges;
    FloatArray spot_light_angles;
    FloatArray spot_light_exponents;
    Mat4fArray spot_lights_shadow_matrices;
};

///
/// Component registration
///

struct DeferredRenderingComponent : Component
{
  void load()
  {
#ifndef __EMSCRIPTEN__
    ScenePassFactory::register_scene_pass("G-Buffer", &GBufferPass::create);
    ScenePassFactory::register_scene_pass("Deferred Lighting", &DeferredLightingPass::create);
#endif
  }

  void unload()
  {
#ifndef __EMSCRIPTEN__
    ScenePassFactory::unregister_scene_pass("G-Buffer");
    ScenePassFactory::unregister_scene_pass("Deferred Lighting");
#endif
  }
};

static DeferredRenderingComponent component;

}}}}
