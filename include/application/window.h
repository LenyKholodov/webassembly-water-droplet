#pragma once

#include <memory>
#include <functional>

//forward declarations
struct GLFWwindow;

namespace engine {
namespace application {

/// Keyboard virtual key
enum Key
{
  Key_Unknown = -1,

  Key_Space,
  Key_Apostrophe,
  Key_Comma,
  Key_Minus,
  Key_Period,
  Key_Slash,
  Key_0,
  Key_1,
  Key_2,
  Key_3,
  Key_4,
  Key_5,
  Key_6,
  Key_7,
  Key_8,
  Key_9,
  Key_Semicolon,
  Key_Equal,
  Key_A,
  Key_B,
  Key_C,
  Key_D,
  Key_E,
  Key_F,
  Key_G,
  Key_H,
  Key_I,
  Key_J,
  Key_K,
  Key_L,
  Key_M,
  Key_N,
  Key_O,
  Key_P,
  Key_Q,
  Key_R,
  Key_S,
  Key_T,
  Key_U,
  Key_V,
  Key_W,
  Key_X,
  Key_Y,
  Key_Z,
  Key_LeftBracket,
  Key_Backslash,
  Key_RightBracket,
  Key_GraveAccent,

  Key_Escape,
  Key_Enter,
  Key_Tab,
  Key_Backspace,
  Key_Insert,
  Key_Delete,
  Key_Right,
  Key_Left,
  Key_Down,
  Key_Up,
  Key_PageUp,
  Key_PageDown,
  Key_Home,
  Key_End,
  Key_CapsLock,
  Key_ScrollLock,
  Key_NumLock,
  Key_PrintScreen,
  Key_Pause,
  Key_F1,
  Key_F2,
  Key_F3,
  Key_F4,
  Key_F5,
  Key_F6,
  Key_F7,
  Key_F8,
  Key_F9,
  Key_F10,
  Key_F11,
  Key_F12,

  Key_LeftShift,
  Key_LeftControl,
  Key_LeftAlt,
  Key_LeftSuper,
  Key_RightShift,
  Key_RightControl,
  Key_RightAlt,
  Key_RightSuper,
  Key_Menu,
};

/// Mouse button
enum MouseButton
{
  MouseButton_Unknown = -1,

  MouseButton_Left,
  MouseButton_Right,
  MouseButton_Middle,
};

/// Window abstraction
class Window
{
  public:
    /// Constructor
    Window(const char* title, unsigned int width = 0, unsigned int height = 0);

    /// Window handle
    GLFWwindow* handle() const;

    /// Window width
    int width() const;

    /// Window height
    int height() const;

    /// Window width
    int frame_buffer_width() const;

    /// Window height
    int frame_buffer_height() const;

    /// Close window
    void close();

    /// Should close window
    bool should_close() const;

    /// Swap back buffer and front buffer
    void swap_buffers();

    /// Window keyboard handler
    typedef std::function<void (Key key, bool state)> KeyHandler;

    /// Set keyboard handler
    void set_keyboard_handler(const KeyHandler& key_handler);

    /// Window mouse button handler
    typedef std::function<void (MouseButton button, bool state)> MouseButtonHandler;

    /// Window mouse button handler
    void set_mouse_button_handler(const MouseButtonHandler& mouse_button_handler);

    /// Window mouse move handler
    typedef std::function<void (double x, double y)> MouseMoveHandler;

    /// Window mouse position handler
    void set_mouse_move_handler(const MouseMoveHandler& mouse_move_handler);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
};

}}
