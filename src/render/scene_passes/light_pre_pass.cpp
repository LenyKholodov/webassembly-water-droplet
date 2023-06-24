#include "shared.h"

using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::common;

namespace engine {
namespace render {
namespace scene {
namespace passes {
namespace lpp {

///
/// Constants
///

static const char* GEOMETRY_PASS_PROGRAM_FILE = "media/shaders/lpp_geometry.glsl";

///
/// Geometry pass
///

struct GeometryPass : IScenePass
{
  public:
    GeometryPass(SceneRenderer& renderer, Device& device)
      : lpp_geometry_buffer_width(device.window().frame_buffer_width())
      , lpp_geometry_buffer_height(device.window().frame_buffer_height())
      , lpp_geometry_buffer_program(device.create_program_from_file(GEOMETRY_PASS_PROGRAM_FILE))
      , lpp_geometry_buffer_pass(device.create_pass(lpp_geometry_buffer_program))
      , shared_textures(renderer.textures())
      , shared_frames(renderer.frame_nodes())
      , normals_texture(device.create_texture2d(lpp_geometry_buffer_width, lpp_geometry_buffer_height, PixelFormat_RGBA8, 1))
      , lpp_geometry_buffer_depth(device.create_render_buffer(lpp_geometry_buffer_width, lpp_geometry_buffer_height, PixelFormat_D16))
      , lpp_geometry_buffer_frame_buffer(device.create_frame_buffer())
    {
      engine_log_debug("Creating LPP-GeometryBuffer...");

      shared_frames.insert("lpp_geometry_buffer", frame);

      shared_textures.insert("normalTexture", normals_texture);

      normals_texture.set_min_filter(TextureFilter_Point);

      lpp_geometry_buffer_frame_buffer.attach_color_target(normals_texture);
      lpp_geometry_buffer_frame_buffer.attach_depth_buffer(lpp_geometry_buffer_depth);

      lpp_geometry_buffer_frame_buffer.reset_viewport();

      //lpp_geometry_buffer_pass.set_frame_buffer(lpp_geometry_buffer_frame_buffer);
      lpp_geometry_buffer_pass.set_clear_color(0.0f);
      lpp_geometry_buffer_pass.set_depth_stencil_state(DepthStencilState(true, true, CompareMode_Less));

      engine_log_debug("LPP-GeometryBuffer has been created: %ux%u", lpp_geometry_buffer_width, lpp_geometry_buffer_height);
    }

    ~GeometryPass()
    {
      shared_textures.remove("normalTexture");
      shared_frames.remove("lpp_geometry_buffer");
    }

    static IScenePass* create(SceneRenderer& renderer, Device& device)
    {
      return new GeometryPass(renderer, device);
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

      frame.add_pass(lpp_geometry_buffer_pass);
      context.root_frame_node().add_dependency(frame);
    }

  private:
    void render_mesh(engine::scene::Mesh& mesh, ScenePassContext& context)
    {
        //create mesh data

      RenderableMesh* renderable_mesh = RenderableMesh::get(mesh.mesh(), context);

        //add mesh to pass

      lpp_geometry_buffer_pass.add_mesh(renderable_mesh->mesh, mesh.world_tm(), mesh.first_primitive(), mesh.primitives_count());
    }

  private:
    size_t lpp_geometry_buffer_width;
    size_t lpp_geometry_buffer_height;
    Program lpp_geometry_buffer_program;
    Pass lpp_geometry_buffer_pass;
    TextureList shared_textures;
    FrameNodeList shared_frames;
    Texture normals_texture;
    RenderBuffer lpp_geometry_buffer_depth;
    FrameBuffer lpp_geometry_buffer_frame_buffer;
    SceneVisitor visitor;
    FrameNode frame;
};

///
/// Deferred lighting pass
///
#if 0
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

    void render(ScenePassContext& context)
    {
        //search for G-Buffer frame and add it to dependency list

      if (!lpp_geometry_buffer_frame_initialized)
      {
        lpp_geometry_buffer_frame = context.frame_nodes().get("lpp_geometry_buffer");
        lpp_geometry_buffer_frame_initialized = true;
      }

      frame.add_dependency(lpp_geometry_buffer_frame);

        //traverse scene

      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

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
    FrameNode lpp_geometry_buffer_frame;
    bool lpp_geometry_buffer_frame_initialized = false;
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

#endif

///
/// Component registration
///

struct LightPrepassRenderingComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("LPP-GeometryPass", &GeometryPass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("LPP-GeometryPass");
  }
};

static LightPrepassRenderingComponent component;

}}}}}
