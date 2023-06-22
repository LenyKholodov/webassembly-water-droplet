#include <scene/camera.h>
#include <scene/mesh.h>
#include <scene/light.h>
#include <media/image.h>
#include <media/sound_player.h>
#include <application/application.h>
#include <application/window.h>
#include <common/exception.h>
#include <common/log.h>
#include <common/component.h>
#include <math/utility.h>

#include "shared.h"

#include <string>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cmath>

using namespace engine::common;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::application;
using namespace engine::scene;
using namespace engine;
using namespace engine::media::sound;

namespace
{

const float CAMERA_MOVE_SPEED = 10.f;
const float CAMERA_ROTATE_SPEED = 0.5f;
const float FOV_X = 90.f;
const math::vec3f LIGHTS_ATTENUATION(1, 0.75, 0.25);
const size_t LIGHTS_COUNT = 32;
const float LIGHTS_POSITION_RADIUS = 30.f;
const float LIGHTS_MIN_INTENSITY = 0.25f;
const float LIGHTS_MAX_INTENSITY = 1.25f;
const float LIGHTS_MIN_RANGE = 10.f;
const float LIGHTS_MAX_RANGE = 50.f;
const size_t MESHES_COUNT = 100;
const float MESHES_POSITION_RADIUS = 3.f;
const float DRAG_OFFSET_MULTIPLIER = 10.f;

float frand()
{
  return rand() / float(RAND_MAX);
}

float crand(float min=-1.0f, float max=1.0f)
{
  return frand() * (max - min) + min;
}

}

int main(void)
{
  try
  {
    engine_log_info("Application has been started");

      //components loading

    ComponentScope components("engine::render::scene::passes::*");

      //application setup

    PerspectiveCamera::Pointer camera = PerspectiveCamera::create();
    math::vec3f camera_position(0.f, 10.f, -10.f);
    math::anglef camera_pitch(math::degree(30.f));
    math::anglef camera_yaw(math::degree(0.f));
    math::anglef camera_roll(math::degree(0.f));
    math::vec3f camera_move_direction(0.f);
    SoundPlayer sound_player;

    Application app;
    Window window("Render test", 1280, 720);

    window.set_keyboard_handler([&](Key key, bool pressed) {
      sound_player.play_music();

      math::vec3f direction_change;

      bool camera_position_changed = false;

      switch (key)
      {
        case Key_Up:
        case Key_W:
          direction_change = math::vec3f(0.f,0.f,pressed ? 1.f : -1.f);
          camera_position_changed = true;
          break;
        case Key_Down:
        case Key_S:
          direction_change = math::vec3f(0.f,0.f,pressed ? -1.f : 1.f);
          camera_position_changed = true;
          break;
        case Key_Right:
        case Key_D:
          direction_change = math::vec3f(pressed ? -1.f : 1.f,0.f,0.f);
          camera_position_changed = true;
          break;
        case Key_Left:
        case Key_A:
          direction_change = math::vec3f(pressed ? 1.f : -1.f,0.f,0.f);
          camera_position_changed = true;
          break;
        case Key_Escape:
          engine_log_info("Escape pressed. Exiting...");
          window.close();
          break;
        default:
          break;
      }

      camera_move_direction += direction_change;
    });

    float window_ratio = window.width() / (float) window.height();

      //scene setup

    Node::Pointer scene_root = Node::create();

    camera->set_z_near(1.f);
    camera->set_z_far(1000.f);
    camera->set_fov_x(math::degree(FOV_X));
    camera->set_fov_y(math::degree(FOV_X / window_ratio));
    camera->set_position(camera_position);
    camera->set_orientation(math::to_quat(camera_pitch, camera_yaw, camera_roll));

    camera->bind_to_parent(*scene_root);

      //scene lights

    Node::Pointer lights_parent = Node::create();

    lights_parent->bind_to_parent(*scene_root);

    std::vector<scene::PointLight::Pointer> point_lights;
    std::vector<math::vec3f> point_lights_center_positions;

    point_lights.reserve(LIGHTS_COUNT);
    point_lights_center_positions.reserve(LIGHTS_COUNT);

    for (size_t i = 0, count = LIGHTS_COUNT; i < count; i++)
    {
      scene::PointLight::Pointer light = scene::PointLight::create();

      point_lights_center_positions.push_back(math::vec3f(LIGHTS_POSITION_RADIUS * cos(math::constf::pi * 2.f * i / count), 0.f, LIGHTS_POSITION_RADIUS * sin(math::constf::pi * 2.f * i / count)));

      light->set_light_color(math::vec3f(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY), crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY)));
      light->set_attenuation(LIGHTS_ATTENUATION);
      light->set_intensity(crand(LIGHTS_MIN_INTENSITY, LIGHTS_MAX_INTENSITY));
      light->set_range(crand(LIGHTS_MIN_RANGE, LIGHTS_MAX_RANGE));

      light->bind_to_parent(*lights_parent);

      point_lights.emplace_back(light);
    }

    scene::SpotLight::Pointer spot_light = scene::SpotLight::create();

    spot_light->set_attenuation(LIGHTS_ATTENUATION);
    spot_light->set_range(100.f);
    spot_light->set_angle(math::degree(30.f));
    spot_light->set_exponent(0.8f);
    spot_light->set_position(math::vec3f(-10.f, 10.f, 0.f));
    spot_light->bind_to_parent(*lights_parent);
    spot_light->world_look_to(math::vec3f(0.0f), math::vec3f(0, 1, 0));

    media::geometry::Mesh spot_light_helper_mesh = media::geometry::MeshFactory::create_box("mtl1", 0.5f, 0.5f, 0.5f);
    scene::Mesh::Pointer spot_light_helper = scene::Mesh::create();

    spot_light_helper->set_mesh(spot_light_helper_mesh);
    spot_light_helper->bind_to_parent(*spot_light);

      //projectile

    scene::PerspectiveProjectile::Pointer projectile = scene::PerspectiveProjectile::create();

    projectile->set_image("media/textures/projectile.png");
    projectile->set_z_near(1.f);
    projectile->set_z_far(100.f);
    projectile->set_fov_x(math::degree(FOV_X));
    projectile->set_fov_y(math::degree(FOV_X / window_ratio));
    projectile->set_position(math::vec3f(10.f, 30.f, 0.f));
    projectile->bind_to_parent(*scene_root);
    projectile->world_look_to(math::vec3f(0.0f), math::vec3f(0, 1, 0));

    media::geometry::Mesh projectile_helper_mesh = media::geometry::MeshFactory::create_sphere("mtl1", .15f);
    scene::Mesh::Pointer projectile_helper = scene::Mesh::create();

    projectile_helper->set_mesh(projectile_helper_mesh);
    projectile_helper->bind_to_parent(*projectile);

      //render setup

    DeviceOptions render_options;

    SceneRenderer scene_renderer(window, render_options);
    Device render_device = scene_renderer.device();

    bool passes_initialized = false;

      //resources creation

    Texture model_diffuse_texture = render_device.create_texture2d("media/textures/brickwall_diffuse.jpg");
    Texture model_normal_texture = render_device.create_texture2d("media/textures/brickwall_normal.jpg");
    Texture model_specular_texture = render_device.create_texture2d("media/textures/brickwall_specular.jpg");

    model_diffuse_texture.set_min_filter(TextureFilter_LinearMipLinear);
    model_normal_texture.set_min_filter(TextureFilter_LinearMipLinear);
    model_specular_texture.set_min_filter(TextureFilter_LinearMipLinear);

    model_diffuse_texture.set_min_filter(TextureFilter_LinearMipLinear);
    model_normal_texture.set_min_filter(TextureFilter_LinearMipLinear);
    model_specular_texture.set_min_filter(TextureFilter_LinearMipLinear);

    Material mtl1;
    PropertyMap mtl1_properties = mtl1.properties();

    mtl1_properties.set("shininess", 10.f);

    TextureList mtl1_textures = mtl1.textures();

    mtl1_textures.insert("diffuseTexture", model_diffuse_texture);
    mtl1_textures.insert("normalTexture", model_normal_texture);
    mtl1_textures.insert("specularTexture", model_specular_texture);

    MaterialList materials = scene_renderer.materials();

    materials.insert("mtl1", mtl1);

      //create world

    World world(scene_root, scene_renderer);

      //scene viewport setup

    SceneViewport scene_viewport = scene_renderer.create_window_viewport();

    scene_viewport.set_view_node(camera);
    scene_viewport.set_clear_color(math::vec4f(0.0f, 0.0f, 0.0f, 1.0f));

    bool left_mouse_button_pressed = false;
    bool right_mouse_button_pressed = false;
    double last_mouse_x;
    double last_mouse_y;
    float target_offset_x, target_offset_y, target_offset_z;

    window.set_mouse_move_handler([&](double x, double y) {
      //double relative_x = x / window.width();
      //double relative_y = y / window.height();

      //engine_log_info("mouse move pos=(%.1f, %.1f) <-> (%.2f, %.2f)", x, y, relative_x, relative_y);

      double dx = x - last_mouse_x;
      double dy = y - last_mouse_y;

      if (right_mouse_button_pressed)
      {
        camera_pitch += math::degree(dy * CAMERA_ROTATE_SPEED);
        camera_yaw -= math::degree(dx * CAMERA_ROTATE_SPEED);

        camera->set_orientation(math::to_quat(camera_pitch, camera_yaw, camera_roll));
      }

      if (left_mouse_button_pressed)
      {
        //compute inverse view projection matrix
        math::mat4f inverse_view_projection = math::inverse(camera->projection_matrix() * math::inverse(camera->world_tm() * scene_viewport.subview_tm()));

        //compute world-space offset for last drag
        float normalized_dx = dx / window.width(),
              normalized_dy = -dy / window.height();

        math::vec4f center_world = inverse_view_projection * math::vec4f(0, 0, -1.f, 1.f),
                    offset_world = inverse_view_projection * math::vec4f(normalized_dx, normalized_dy, -1.f, 1.f);

        center_world /= center_world.w;
        offset_world /= offset_world.w;

        target_offset_x += (offset_world.x - center_world.x) * DRAG_OFFSET_MULTIPLIER;
        target_offset_y += (offset_world.y - center_world.y) * DRAG_OFFSET_MULTIPLIER;
        target_offset_z += (offset_world.z - center_world.z) * DRAG_OFFSET_MULTIPLIER;
      }

      last_mouse_x = x;
      last_mouse_y = y;
    });

    window.set_mouse_button_handler([&](MouseButton button, bool pressed) {
      //engine_log_info("mouse button=%d pressed=%d", button, pressed);
      sound_player.play_music();

      if (pressed)
        sound_player.play_sound(SoundId::drop);

      if (button == MouseButton_Left)
      {
        left_mouse_button_pressed = pressed;

        if (pressed)
        {
          //compute inverse view projection matrix
          math::mat4f inverse_view_projection = math::inverse(camera->projection_matrix() * math::inverse(camera->world_tm() * scene_viewport.subview_tm()));

          //transorm last_mouse_x, last_mouse_y to world space
          float normalized_x = (last_mouse_x / window.width()) * 2.f - 1.f,
                normalized_y = 1.f - (last_mouse_y / window.height()) * 2.f;

          math::vec4f ray_start = inverse_view_projection * math::vec4f(normalized_x, normalized_y, -1.f, 1.f),
                      ray_end = inverse_view_projection * math::vec4f(normalized_x, normalized_y, 1.f, 1.f);

          ray_start /= ray_start.w;
          ray_end /= ray_end.w;

          world.inputGrab(ray_start.x, ray_start.y, ray_start.z, ray_end.x, ray_end.y, ray_end.z);

          target_offset_x = 0.f;
          target_offset_y = 0.f;
          target_offset_z = 0.f;

/*          math::vec4f camera_forward = camera->world_tm() * math::vec4f(0, 0, 1, 1);

          engine_log_info("cursor pos = (%.2f, %.2f)", normalized_x, normalized_y);
          engine_log_info("camera pos = (%.2f, %.2f, %.2f)", camera->position().x, camera->position().y, camera->position().z);
          engine_log_info("camera forward = (%.2f, %.2f, %.2f)", camera_forward.x, camera_forward.y, camera_forward.z);
          engine_log_info("ray_start=(%.2f, %.2f, %.2f, %.2f) ray_end=(%.2f, %.2f, %.2f, %.2f)",
                          ray_start.x, ray_start.y, ray_start.z, ray_start.w,
                          ray_end.x, ray_end.y, ray_end.z, ray_end.w);*/
        }
        else
          world.inputRelease();
      }

      if (button == MouseButton_Right)
        right_mouse_button_pressed = pressed;
    });

      //main loop

    double last_time = app.time();

    app.main_loop([&]()
    {
      if (window.should_close())
        app.exit();

      world.inputDrag(target_offset_x, target_offset_y, target_offset_z);
      world.update();

      if (!passes_initialized)
      {
        scene_renderer.add_pass("Forward Lighting");
        scene_renderer.add_pass("Mirrors");
        //scene_renderer.add_pass("LPP-GeometryPass");
        //scene_renderer.add_pass("Deferred Lighting");
        //scene_renderer.add_pass("Projectile Maps Rendering");

        passes_initialized = true;
      }

      double new_time = app.time();
      double dt = new_time - last_time;

      last_time = new_time;

      if (!math::equal(camera_move_direction, math::vec3f(0.f), 0.1f))
      {
        camera_position += math::to_quat(camera_pitch, camera_yaw, camera_roll) * camera_move_direction * CAMERA_MOVE_SPEED * dt;

        camera->set_position(camera_position);
      }

        //animate objects

      float time = Application::time();

      for (size_t i=0; i<point_lights.size(); i++)
      {
        auto& light = point_lights[i];
        float factor = math::constf::pi * 2.f * i / point_lights.size();

        math::vec3f dpos = to_quat(math::degree(factor + time * 100 * factor), math::vec3f(0, 1, 0)) * math::vec3f(10, 0, 0);
        math::vec3f pos = point_lights_center_positions[i] + dpos;

        light->set_position(pos);
      }

      spot_light->set_intensity((1.0f + cos(time * 2)) / 2.0f * 10.0f + 0.25f);

      spot_light->set_position(math::vec3f(cos(time * 0.5) * 10, 10.f, sin(time * 0.5) * 10));

      projectile->set_position(math::vec3f(sin(time * 0.3) * 10, 5.f, cos(time * 0.6) * 8));
      projectile->set_intensity((1.0f + cos(time)) / 2.0f * 10.0f + 0.25f);

        //render scene

      scene_renderer.render(scene_viewport);

        //image presenting

      window.swap_buffers();

        //wait for next frame

      static const size_t TIMEOUT_MS = 10;

      return TIMEOUT_MS;
    });

    engine_log_info("Exiting from application...");

    return 0;
  }
  catch (std::exception& e)
  {
    engine_log_fatal("%s\n", e.what());
    return 1;
  }
}
