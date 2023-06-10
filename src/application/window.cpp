#include <application/window.h>
#include <common/exception.h>
#include <common/log.h>
#include <string>

extern "C"
{
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
}

using namespace engine::application;
using namespace engine::common;

namespace
{

Key map_from_glfw_key(int key)
{
  switch (key)
  {
    default:
    case GLFW_KEY_UNKNOWN: return Key_Unknown;
    case GLFW_KEY_SPACE: return Key_Space;
    case GLFW_KEY_APOSTROPHE: return Key_Apostrophe;
    case GLFW_KEY_COMMA: return Key_Comma;
    case GLFW_KEY_MINUS: return Key_Minus;
    case GLFW_KEY_PERIOD: return Key_Period;
    case GLFW_KEY_SLASH: return Key_Slash;
    case GLFW_KEY_0: return Key_0;
    case GLFW_KEY_1: return Key_1;
    case GLFW_KEY_2: return Key_2;
    case GLFW_KEY_3: return Key_3;
    case GLFW_KEY_4: return Key_4;
    case GLFW_KEY_5: return Key_5;
    case GLFW_KEY_6: return Key_6;
    case GLFW_KEY_7: return Key_7;
    case GLFW_KEY_8: return Key_8;
    case GLFW_KEY_9: return Key_9;
    case GLFW_KEY_SEMICOLON: return Key_Semicolon;
    case GLFW_KEY_EQUAL: return Key_Equal;
    case GLFW_KEY_A: return Key_A;
    case GLFW_KEY_B: return Key_B;
    case GLFW_KEY_C: return Key_C;
    case GLFW_KEY_D: return Key_D;
    case GLFW_KEY_E: return Key_E;
    case GLFW_KEY_F: return Key_F;
    case GLFW_KEY_G: return Key_G;
    case GLFW_KEY_H: return Key_H;
    case GLFW_KEY_I: return Key_I;
    case GLFW_KEY_J: return Key_J;
    case GLFW_KEY_K: return Key_K;
    case GLFW_KEY_L: return Key_L;
    case GLFW_KEY_M: return Key_M;
    case GLFW_KEY_N: return Key_N;
    case GLFW_KEY_O: return Key_O;
    case GLFW_KEY_P: return Key_P;
    case GLFW_KEY_Q: return Key_Q;
    case GLFW_KEY_R: return Key_R;
    case GLFW_KEY_S: return Key_S;
    case GLFW_KEY_T: return Key_T;
    case GLFW_KEY_U: return Key_U;
    case GLFW_KEY_V: return Key_V;
    case GLFW_KEY_W: return Key_W;
    case GLFW_KEY_X: return Key_X;
    case GLFW_KEY_Y: return Key_Y;
    case GLFW_KEY_Z: return Key_Z;
    case GLFW_KEY_LEFT_BRACKET: return Key_LeftBracket;
    case GLFW_KEY_BACKSLASH: return Key_Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return Key_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return Key_GraveAccent;
    case GLFW_KEY_ESCAPE: return Key_Escape;
    case GLFW_KEY_ENTER: return Key_Enter;
    case GLFW_KEY_TAB: return Key_Tab;
    case GLFW_KEY_BACKSPACE: return Key_Backspace;
    case GLFW_KEY_INSERT: return Key_Insert;
    case GLFW_KEY_DELETE: return Key_Delete;
    case GLFW_KEY_RIGHT: return Key_Right;
    case GLFW_KEY_LEFT: return Key_Left;
    case GLFW_KEY_DOWN: return Key_Down;
    case GLFW_KEY_UP: return Key_Up;
    case GLFW_KEY_PAGE_UP: return Key_PageUp;
    case GLFW_KEY_PAGE_DOWN: return Key_PageDown;
    case GLFW_KEY_HOME: return Key_Home;
    case GLFW_KEY_END: return Key_End;
    case GLFW_KEY_CAPS_LOCK: return Key_CapsLock;
    case GLFW_KEY_SCROLL_LOCK: return Key_ScrollLock;
    case GLFW_KEY_NUM_LOCK: return Key_NumLock;
    case GLFW_KEY_PRINT_SCREEN: return Key_PrintScreen;
    case GLFW_KEY_PAUSE: return Key_Pause;
    case GLFW_KEY_F1: return Key_F1;
    case GLFW_KEY_F2: return Key_F2;
    case GLFW_KEY_F3: return Key_F3;
    case GLFW_KEY_F4: return Key_F4;
    case GLFW_KEY_F5: return Key_F5;
    case GLFW_KEY_F6: return Key_F6;
    case GLFW_KEY_F7: return Key_F7;
    case GLFW_KEY_F8: return Key_F8;
    case GLFW_KEY_F9: return Key_F9;
    case GLFW_KEY_F10: return Key_F10;
    case GLFW_KEY_F11: return Key_F11;
    case GLFW_KEY_F12: return Key_F12;
    case GLFW_KEY_LEFT_SHIFT: return Key_LeftShift;
    case GLFW_KEY_LEFT_CONTROL: return Key_LeftControl;
    case GLFW_KEY_LEFT_ALT: return Key_LeftAlt;
    case GLFW_KEY_LEFT_SUPER: return Key_LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT: return Key_RightShift;
    case GLFW_KEY_RIGHT_CONTROL: return Key_RightControl;
    case GLFW_KEY_RIGHT_ALT: return Key_RightAlt;
    case GLFW_KEY_RIGHT_SUPER: return Key_RightSuper;
    case GLFW_KEY_MENU: return Key_Menu;
  }
}

MouseButton map_from_glfw_mouse_button(int button)
{
  switch (button)
  {
    case GLFW_MOUSE_BUTTON_1: return MouseButton_Left;
    case GLFW_MOUSE_BUTTON_2: return MouseButton_Right;
    case GLFW_MOUSE_BUTTON_3: return MouseButton_Middle;
    default:                  return MouseButton_Unknown;
  }
}

}

/// Window implementation details
struct Window::Impl
{
  std::string title; //window title
  GLFWwindow* window; //window handle
  KeyHandler key_handler; //keyboard handler
  MouseButtonHandler mouse_button_handler; //mouse button handler
  MouseMoveHandler mouse_move_handler; //mouse move handler

  Impl(const char* in_title, unsigned int width, unsigned int height)
    : title(in_title)
    , window()
  {
    engine_log_info("Creating window '%s' %ux%u...", title.c_str(), width, height);

      //setting minimal OpenGL version 4.1
      //TODO: configuration

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true); 
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, false);

      //if width and height was not requested, create full screen window
    if (!width && !height)
    {
      const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

      width = mode->width / 2;
      height = mode->height / 2;
    }

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!window)
    {
      const char* error = "";
      glfwGetError(&error);

      throw Exception::format("GLFW window creation error: %s", error);
    }

    glfwSetWindowUserPointer(window, this);

    glfwSetKeyCallback(window, key_callback_static);
    glfwSetMouseButtonCallback(window, mouse_button_callback_static);
    glfwSetCursorPosCallback(window, mouse_move_callback_static);
  }

  ~Impl()
  {
    engine_log_info("Destroying window '%s'", title.c_str());

    glfwSetWindowUserPointer(window, nullptr);
    glfwDestroyWindow(window);
  }

  static void key_callback_static(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    Impl* impl = reinterpret_cast<Impl*>(glfwGetWindowUserPointer(window));

    if (!impl)
      return;

    impl->key_callback(key, action);
  }

  void key_callback(int glfw_key, int action)
  {
    if (action == GLFW_REPEAT)
      return;

    Key key = map_from_glfw_key(glfw_key);
    bool pressed = action == GLFW_PRESS;

    try
    {
      if (!key_handler)
        return;

      key_handler(key, pressed);
    }
    catch (std::exception& e)
    {
      engine_log_error("keyboard handler: %s", e.what());
    }
  }

  static void mouse_button_callback_static(GLFWwindow* window, int button, int action, int mods)
  {
    Impl* impl = reinterpret_cast<Impl*>(glfwGetWindowUserPointer(window));

    if (!impl)
      return;

    impl->mouse_button_callback(button, action);
  }

  void mouse_button_callback(int glfw_button, int action)
  {
    if (action == GLFW_REPEAT)
      return;

    MouseButton button = map_from_glfw_mouse_button(glfw_button);
    bool pressed = action == GLFW_PRESS;

    try
    {
      if (!mouse_button_handler)
        return;

      mouse_button_handler(button, pressed);
    }
    catch (std::exception& e)
    {
      engine_log_error("mouse button handler: %s", e.what());
    }
  }

  static void mouse_move_callback_static(GLFWwindow* window, double x, double y)
  {
    Impl* impl = reinterpret_cast<Impl*>(glfwGetWindowUserPointer(window));

    if (!impl)
      return;

    impl->mouse_move_callback(x, y);
  }

  void mouse_move_callback(double x, double y)
  {
    try
    {
      if (!mouse_move_handler)
        return;

      mouse_move_handler(x, y);
    }
    catch (std::exception& e)
    {
      engine_log_error("mouse move handler: %s", e.what());
    }
  }
};

Window::Window(const char* title, unsigned int width, unsigned int height)
{
  if (!title)
    throw make_null_argument_exception("title");

  impl.reset(new Impl(title, width, height));
}

GLFWwindow* Window::handle() const
{
  return impl->window;
}

int Window::width() const
{
  int width = 0, height = 0;

  glfwGetWindowSize(impl->window, &width, &height);

  return width;
}

int Window::height() const
{
  int width = 0, height = 0;

  glfwGetWindowSize(impl->window, &width, &height);

  return height;
}

int Window::frame_buffer_width() const
{
  int width = 0, height = 0;

  glfwGetFramebufferSize(impl->window, &width, &height);

  return width;
}

int Window::frame_buffer_height() const
{
  int width = 0, height = 0;

  glfwGetFramebufferSize(impl->window, &width, &height);

  return height;
}

void Window::close()
{
  glfwSetWindowShouldClose(impl->window, GLFW_TRUE);
}

bool Window::should_close() const
{
  return glfwWindowShouldClose(impl->window);
}

void Window::swap_buffers()
{
  glfwSwapBuffers(impl->window);
}

void Window::set_keyboard_handler(const KeyHandler& key_handler)
{
  impl->key_handler = key_handler;
}

void Window::set_mouse_button_handler(const MouseButtonHandler& mouse_button_handler)
{
  impl->mouse_button_handler = mouse_button_handler;
}

void Window::set_mouse_move_handler(const MouseMoveHandler& mouse_move_handler)
{
  impl->mouse_move_handler = mouse_move_handler;
}
