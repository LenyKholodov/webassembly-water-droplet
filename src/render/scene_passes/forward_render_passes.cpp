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

static const char* FORWARD_LIGHTING_PROGRAM_FILE = "media/shaders/forward_lighting.glsl";
static const char* FRESNEL_PROGRAM_FILE = "media/shaders/fresnel.glsl";
static const char* SKY_PROGRAM_FILE = "media/shaders/sky.glsl";

///
/// Forward lighting pass
///

struct ForwardLightingPass : IScenePass
{
  public:
    ForwardLightingPass(SceneRenderer& renderer, Device& device)
      : forward_lighting_program(device.create_program_from_file(FORWARD_LIGHTING_PROGRAM_FILE))
      , fresnel_program(device.create_program_from_file(FRESNEL_PROGRAM_FILE))
      , sky_program(device.create_program_from_file(SKY_PROGRAM_FILE))
      , forward_lighting_pass(device.create_pass(forward_lighting_program))
      , fresnel_pass(device.create_pass(fresnel_program))
      , sky_pass(device.create_pass(sky_program))
    {
      forward_lighting_pass.set_depth_stencil_state(DepthStencilState(true, true, CompareMode_Less));

      fresnel_pass.set_depth_stencil_state(DepthStencilState(true, true, CompareMode_Less));
      fresnel_pass.set_clear_flags(Clear_None);

      sky_pass.set_depth_stencil_state(DepthStencilState(true, true, CompareMode_Less));
      sky_pass.set_rasterizer_state(RasterizerState(false));
      sky_pass.set_clear_flags(Clear_None);

      size_t default_pass_index = pass_group.add_pass(nullptr, forward_lighting_pass, 0);
      pass_group.add_pass("fresnel", fresnel_pass, 1);
      pass_group.add_pass("sky", sky_pass, 2);
      pass_group.set_default_pass(default_pass_index);

      engine_log_debug("Forward Lighting pass has been created");
    }

    static IScenePass* create(SceneRenderer& renderer, Device& device)
    {
      return new ForwardLightingPass(renderer, device);
    }

    void get_dependencies(std::vector<std::string>& deps)
    {
      deps.push_back("Shadow Maps Rendering");
      //deps.push_back("Projectile Maps Rendering");
    }

    void prerender(ScenePassContext& context)
    {
    }

    void render(ScenePassContext& context)
    {
        //traverse scene

      Node::Pointer root_node = context.root_node();

      if (!root_node)
        return;

        //configure framebuffer

      forward_lighting_pass.set_frame_buffer(context.default_frame_buffer());
      forward_lighting_pass.set_clear_color(context.clear_color());
      fresnel_pass.set_frame_buffer(context.default_frame_buffer());
      sky_pass.set_frame_buffer(context.default_frame_buffer());

        //clean pass

      forward_lighting_pass.remove_all_primitives();
      fresnel_pass.remove_all_primitives();
      sky_pass.remove_all_primitives();

        //traverse scene

      visitor.traverse(*root_node, &context.options());

        //configure params

      setup_point_lights(visitor.point_lights(), context);
      setup_spot_lights(visitor.spot_lights(), context);

        //draw geometry

      for (auto& mesh : visitor.meshes())
      {
        render_mesh(*mesh, context);
      }

        //add this frame to root frame

      frame.add_pass_group(pass_group);
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
    void render_mesh(engine::scene::Mesh& mesh, ScenePassContext& context)
    {
        //create mesh data

      RenderableMesh* renderable_mesh = RenderableMesh::get(mesh.mesh(), context);

        //check for envmap

      EnvironmentMap* envmap = EnvironmentMap::find(mesh);

        //add mesh to pass

      pass_group.add_mesh(renderable_mesh->mesh, mesh.world_tm(), mesh.first_primitive(), mesh.primitives_count(), Pass::default_primitive_properties(),
        envmap ? envmap->textures : Pass::default_primitive_textures());
    }

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
        forward_lighting_pass.textures().remove("shadowTexture");
        forward_lighting_pass.textures().insert("shadowTexture", shadow->shadow_texture);
        fresnel_pass.textures().remove("shadowTexture");
        fresnel_pass.textures().insert("shadowTexture", shadow->shadow_texture);

        float tex_size_step = 1.0f / shadow->shadow_texture.width();

        forward_lighting_pass.properties().set("shadowMapPixelSize", math::vec2f(tex_size_step));
        fresnel_pass.properties().set("shadowMapPixelSize", math::vec2f(tex_size_step));

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
    Program forward_lighting_program;
    Program fresnel_program;
    Program sky_program;
    Pass forward_lighting_pass;
    Pass fresnel_pass;
    Pass sky_pass;
    PassGroup pass_group;
    FrameNode frame;    
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

struct ForwardRenderingComponent : Component
{
  void load()
  {
    ScenePassFactory::register_scene_pass("Forward Lighting", &ForwardLightingPass::create);
  }

  void unload()
  {
    ScenePassFactory::unregister_scene_pass("Forward Lighting");
  }
};

static ForwardRenderingComponent component;

}}}}
