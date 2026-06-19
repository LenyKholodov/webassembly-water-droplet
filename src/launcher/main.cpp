#include <scene/camera.h>
#include <scene/mesh.h>
#include <scene/light.h>
#include <media/image.h>
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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

using namespace engine::common;
using namespace engine::render::scene;
using namespace engine::render::low_level;
using namespace engine::scene;
using namespace engine::application;
using namespace engine::scene;
using namespace engine;

namespace
{

const float CAMERA_MOVE_SPEED = 10.f;
const float CAMERA_ROTATE_SPEED = 0.5f;
// cylinder-orbit camera tuning. The orbit axis sits between the two stems (leaf_1/leaf_2 in leaf.obj,
// at world ~(-1.67,2.34) and ~(-1.93,-4.69)); each is ~3.5 from this midpoint, so the radius stays
// below that and the camera rides between the trees with a trunk always behind it.
const float CAM_CENTER_X     = -1.80f;  // midpoint between the two stems (x)
const float CAM_CENTER_Z     = -1.18f;  // midpoint between the two stems (z)
const float CAM_ORBIT_SPEED  = 0.006f;  // radians of azimuth per pixel of horizontal drag
const float CAM_HEIGHT_SPEED = 0.025f;  // world units of height per pixel of vertical drag
const float CAM_HEIGHT_MIN   = -5.f;    // toward the water
const float CAM_HEIGHT_MAX   = 8.f;     // above the canopy
const float CAM_RADIUS_MIN   = 0.8f;    // closest zoom
const float CAM_RADIUS_MAX   = 3.3f;    // farthest zoom (< ~3.5 trunk distance -> stays inside the two trees)
const float CAM_LOOK_DOWN    = 1.6f;    // look this much below the camera height -> tilts down to follow falling droplets
const float CAM_WHEEL_ZOOM   = 0.15f;   // radius change per wheel notch (desktop)
const float CAM_PINCH_ZOOM   = 0.008f;  // radius change per pixel of pinch-distance change
const float FOV_X_LANDSCAPE = 90.f;
const float FOV_Y_PORTRAIT = 90.f;
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
const math::vec3f CAM_POS_AR_16_9(18.f, 12.f, -1.f);
const math::vec3f CAM_POS_AR_1_1(9.f, 6.f, -1.f);
const math::vec3f CAM_POS_AR_9_16(12.f, 8.f, -1.f);

float frand()
{
  return rand() / float(RAND_MAX);
}

float crand(float min=-1.0f, float max=1.0f)
{
  return frand() * (max - min) + min;
}

}

#ifdef __EMSCRIPTEN__
EM_JS(int, canvas_get_width, (), {
  return canvas.clientWidth;
});

EM_JS(int, canvas_get_height, (), {
  return canvas.clientHeight;
});
#endif

int main(void)
{
  try
  {
    engine_log_info("Application has been started");

      //components loading

    ComponentScope components("engine::render::scene::passes::*");

      //application setup

    PerspectiveCamera::Pointer camera = PerspectiveCamera::create();
    // cylinder-orbit camera: the camera rides a vertical cylinder around the central axis (x=z=0) and
    // always looks at that axis. Drag spirals it around+up/down the cylinder; pinch/wheel changes radius.
    float cam_angle  = 0.0f;   // azimuth around the axis (radians)
    float cam_height = 1.0f;   // height along the axis (world Y) - around the fall zone
    float cam_radius = 2.5f;   // distance from the axis (zoom), kept inside the two trees
    SoundPlayer sound_player;

    Application app;

    int window_width = 1280;
    int window_height = 720;

#ifdef __EMSCRIPTEN__
    window_width = canvas_get_width();
    window_height = canvas_get_height();
#endif

    float window_ratio = window_width / (float) window_height;

    if (window_ratio > 1)
    {
      camera->set_fov_x(math::degree(FOV_X_LANDSCAPE));
      camera->set_fov_y(math::degree(FOV_X_LANDSCAPE / window_ratio));
    }
    else
    {
      camera->set_fov_x(math::degree(FOV_Y_PORTRAIT * window_ratio));
      camera->set_fov_y(math::degree(FOV_Y_PORTRAIT));
    }

    engine_log_info("Window size: %dx%d", window_width, window_height);

    Window window("Render test", window_width, window_height);

    window.set_keyboard_handler([&](Key key, bool pressed) {
      sound_player.play_music();

      if (key == Key_Escape)
      {
        engine_log_info("Escape pressed. Exiting...");
        window.close();
      }
    });

      //scene setup

    Node::Pointer scene_root = Node::create();

    camera->set_z_near(1.f);
    camera->set_z_far(1000.f);

    // place the camera on a cylinder around the axis between the two trees, looking at that axis a bit
    // lower than the camera (tilt down) so falling droplets are in focus (recomputed every frame)
    auto apply_cylinder_camera = [&]() {
      camera->set_position(math::vec3f(CAM_CENTER_X + cam_radius * std::sin(cam_angle), cam_height, CAM_CENTER_Z + cam_radius * std::cos(cam_angle)));
      camera->world_look_to(math::vec3f(CAM_CENTER_X, cam_height - CAM_LOOK_DOWN, CAM_CENTER_Z), math::vec3f(0.0f, 1.0f, 0.0f));
    };
    apply_cylinder_camera();

    camera->bind_to_parent(*scene_root);

      //scene lights

    Node::Pointer lights_parent = Node::create();

    lights_parent->bind_to_parent(*scene_root);

    scene::SpotLight::Pointer spot_light = scene::SpotLight::create();

    //spot_light->set_attenuation(LIGHTS_ATTENUATION);
    spot_light->set_range(45.f);
    spot_light->set_angle(math::degree(55.f));
    spot_light->set_intensity(100.5f);
    spot_light->set_exponent(0.35f);                                  // soft falloff
    spot_light->set_light_color(math::vec3f(0.50f, 0.62f, 0.88f));    // cool moonlight tint
    //spot_light->set_position(math::vec3f(-20.f, 20.f, 0.f));
    //spot_light->set_position(math::vec3f(0.f, 3.f, 0.f));
    spot_light->bind_to_parent(*lights_parent);
    //spot_light->bind_to_parent(*camera);
    //spot_light->set_orientation(to_quat(math::rotate(math::degree(30.f), math::vec3f(1.f, 0.f, 0.f))));
    //spot_light->world_look_to(math::vec3f(0.0f), math::vec3f(0, 1, 0));
    //spot_light->world_look_to(math::vec3f(0.0f), math::vec3f(0, 1, 0));

    media::geometry::Mesh spot_light_helper_mesh = media::geometry::MeshFactory::create_box("mtl1", 0.5f, 0.5f, 0.5f);
    scene::Mesh::Pointer spot_light_helper = scene::Mesh::create();

    spot_light_helper->set_mesh(spot_light_helper_mesh);
    spot_light_helper->bind_to_parent(*spot_light);

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

    World world(scene_root, scene_renderer, camera);

      //scene viewport setup

    SceneViewport scene_viewport = scene_renderer.create_window_viewport();

    scene_viewport.set_view_node(camera);
    scene_viewport.set_clear_color(math::vec4f(0.0f, 0.0f, 0.0f, 1.0f));

    bool left_mouse_button_pressed = false;
    bool right_mouse_button_pressed = false;
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;

    int start_play_music = 0;

    window.set_mouse_move_handler([&](double x, double y) {
      //double relative_x = x / window.width();
      //double relative_y = y / window.height();

      //engine_log_info("mouse move pos=(%.1f, %.1f) <-> (%.2f, %.2f)", x, y, relative_x, relative_y);
      start_play_music = 30;

      double dx = x - last_mouse_x;
      double dy = y - last_mouse_y;

      // drag (mouse or one finger) spirals the camera over the cylinder surface:
      // horizontal -> orbit around the central axis, vertical -> move up/down the cylinder
      if (left_mouse_button_pressed || right_mouse_button_pressed)
      {
        cam_angle  += (float)(dx * CAM_ORBIT_SPEED);
        cam_height -= (float)(dy * CAM_HEIGHT_SPEED); // drag up -> rise
        cam_height  = std::min(std::max(cam_height, CAM_HEIGHT_MIN), CAM_HEIGHT_MAX);
      }

      last_mouse_x = x;
      last_mouse_y = y;
    });

    window.set_mouse_button_handler([&](MouseButton button, bool pressed) {
      //engine_log_info("mouse button=%d pressed=%d", button, pressed);
      sound_player.play_music();

      start_play_music = 10;

      if (button == MouseButton_Left)
        left_mouse_button_pressed = pressed;  // drives the cylinder orbit in the move handler

      if (button == MouseButton_Right)
        right_mouse_button_pressed = pressed;
    });

    auto zoom = [&](float radius_delta) {
      cam_radius = std::min(std::max(cam_radius + radius_delta, CAM_RADIUS_MIN), CAM_RADIUS_MAX);
    };

    // desktop: mouse wheel zooms (wheel up = closer)
    window.set_wheel_handler([&](double delta) {
      zoom(-(float)delta * CAM_WHEEL_ZOOM);
    });

    // mobile: two-finger pinch zooms (fingers spread = closer)
    window.set_pinch_handler([&](double distance_delta) {
      zoom(-(float)distance_delta * CAM_PINCH_ZOOM);
    });

      //main loop

    double last_time = app.time();

    bool force_music_play_started = false;

    app.main_loop([&]()
    {
      if (window.should_close())
        app.exit();

      //hack for android, on android music won't play until some time is passed after first interaction
      if (start_play_music && start_play_music-- == 1 && !force_music_play_started)
      {
        force_music_play_started = true;
        sound_player.play_music(true);
      }

      double new_time = app.time();
      double dt = new_time - last_time;
      last_time = new_time;

      world.update((float) dt);

      sound_player.update();

      if (!passes_initialized)
      {
        scene_renderer.add_pass("Forward Lighting");
        scene_renderer.add_pass("Mirrors");
        scene_renderer.add_pass("Water Reflection");
        //scene_renderer.add_pass("LPP-GeometryPass");
        //scene_renderer.add_pass("Deferred Lighting");
        //scene_renderer.add_pass("Projectile Maps Rendering");

        passes_initialized = true;
      }

        //place the camera on the cylinder from the current angle/height/radius (updated by drag + zoom)

      apply_cylinder_camera();

        //animate objects

      float time = Application::time();

      static const float SPOT_LIGHT_ROTATION_FREQUENCY = 0.12f; // slow, mysterious drift

      // the flying box is a soft moonlight high above, gently circling and focused on the tree
      spot_light->set_intensity(1.2f);

      spot_light->set_position(math::vec3f(cos(time * SPOT_LIGHT_ROTATION_FREQUENCY) * 9.f, 18.f, sin(time * SPOT_LIGHT_ROTATION_FREQUENCY) * 9.f));
      spot_light->world_look_to(math::vec3f(0.0f), math::vec3f(0, 1, 0));

        //render scene

      scene_renderer.render(scene_viewport);

        //image presenting

      window.swap_buffers();

      //engine_log_debug("campos=(%.2f, %.2f, %.2f)",
      //                 camera->position().x, camera->position().y, camera->position().z);

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
