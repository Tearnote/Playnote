/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/glfw.hpp:
Wrapper for GLFW window and input handling.
*/

#pragma once
#include "preamble.hpp"
#include "lib/vulkan.hpp"

// Forward declaration
struct GLFWwindow;

namespace playnote::lib::glfw {

// Opaque handle to a window.
using Window = GLFWwindow*;

namespace detail {

void set_window_user_pointer_raw(Window window, void* ptr);
[[nodiscard]] auto get_window_user_pointer_raw(Window window) -> void*;
void set_window_key_handler_raw(Window window, void (*func)(Window, int, int, int, int));
void set_window_cursor_motion_handler_raw(Window window, void (*func)(Window, double, double));
void set_window_mouse_button_handler_raw(Window window, void (*func)(Window, int, int, int));

}

// Make GLFW throw on any error. Can be called anytime.
void register_error_handler();

// Initialize GLFW.
// Throws runtime_error on failure.
void init();

// Clean up GLFW.
// Errors are ignored.
void cleanup() noexcept;

// Return the me passed since the call to init_glfw(). If GLFW is not initialized, returns 0.
[[nodiscard]] auto time_since_init() -> nanoseconds;

// Check for all open windows' events, and dispatch registered callbacks.
// Throws runtime_error on failure, or if a callback throws.
void process_events();

// Return the current value of the window's "closing" flag. This flag is set automatically
// if the user presses the "X" in the corner, or programmatically via set_window_closing_flag.
[[nodiscard]] auto get_window_closing_flag(Window window) -> bool;

// Set the current value of the window's "closing" flag.
void set_window_closing_flag(Window window, bool flag_value);

// Set window creation hints to the expected values.
void set_window_creation_hints();

// Open a new window and return the handle. Close it once done.
// Throws runtime_error on failure.
auto create_window(uvec2 size, string_view title) -> Window;

// Destroy a previously opened window.
// Errors are ignored.
void destroy_window(Window window) noexcept;

// Set the custom pointer value that gets passed to event handlers.
template<typename T>
void set_window_user_pointer(Window window, T* ptr) {
	detail::set_window_user_pointer_raw(window, ptr);
}

// Retrieve the previously set custom pointer.
template<typename T>
[[nodiscard]] auto get_window_user_pointer(Window window) -> T*
{
	return static_cast<T*>(detail::get_window_user_pointer_raw(window));
}

// Return the Window's title. String is valid until it's changed by another call, or the window
// is destroyed.
[[nodiscard]] auto get_window_title(Window window) -> string_view;

enum class KeyCode: int {
	Space = 32, // GLFW_KEY_SPACE
	Apostrophe = 39, // GLFW_KEY_APOSTROPHE
	Comma = 44, // GLFW_KEY_COMMA
	Minus = 45, // GLFW_KEY_MINUS
	Period = 46, // GLFW_KEY_PERIOD
	Slash = 47, // GLFW_KEY_SLASH
	Zero = 48, // GLFW_KEY_0
	One = 49, // GLFW_KEY_1
	Two = 50, // GLFW_KEY_2
	Three = 51, // GLFW_KEY_3
	Four = 52, // GLFW_KEY_4
	Five = 53, // GLFW_KEY_5
	Six = 54, // GLFW_KEY_6
	Seven = 55, // GLFW_KEY_7
	Eight = 56, // GLFW_KEY_8
	Nine = 57, // GLFW_KEY_9
	Semicolon = 59, // GLFW_KEY_SEMICOLON
	Equal = 61, // GLFW_KEY_EQUAL
	A = 65, // GLFW_KEY_A
	B = 66, // GLFW_KEY_B
	C = 67, // GLFW_KEY_C
	D = 68, // GLFW_KEY_D
	E = 69, // GLFW_KEY_E
	F = 70, // GLFW_KEY_F
	G = 71, // GLFW_KEY_G
	H = 72, // GLFW_KEY_H
	I = 73, // GLFW_KEY_I
	J = 74, // GLFW_KEY_J
	K = 75, // GLFW_KEY_K
	L = 76, // GLFW_KEY_L
	M = 77, // GLFW_KEY_M
	N = 78, // GLFW_KEY_N
	O = 79, // GLFW_KEY_O
	P = 80, // GLFW_KEY_P
	Q = 81, // GLFW_KEY_Q
	R = 82, // GLFW_KEY_R
	S = 83, // GLFW_KEY_S
	T = 84, // GLFW_KEY_T
	U = 85, // GLFW_KEY_U
	V = 86, // GLFW_KEY_V
	W = 87, // GLFW_KEY_W
	X = 88, // GLFW_KEY_X
	Y = 89, // GLFW_KEY_Y
	Z = 90, // GLFW_KEY_Z
	LeftBracket = 91, // GLFW_KEY_LEFT_BRACKET
	Backslash = 92, // GLFW_KEY_BACKSLASH
	RightBracket = 93, // GLFW_KEY_RIGHT_BRACKET
	GraveAccent = 96, // GLFW_KEY_GRAVE_ACCENT
	World1 = 161, // GLFW_KEY_WORLD_1
	World2 = 162, // GLFW_KEY_WORLD_2
	Escape = 256, // GLFW_KEY_ESCAPE
	Enter = 257, // GLFW_KEY_ENTER
	Tab = 258, // GLFW_KEY_TAB
	Backspace = 259, // GLFW_KEY_BACKSPACE
	Insert = 260, // GLFW_KEY_INSERT
	Delete = 261, // GLFW_KEY_DELETE
	Right = 262, // GLFW_KEY_RIGHT
	Left = 263, // GLFW_KEY_LEFT
	Down = 264, // GLFW_KEY_DOWN
	Up = 265, // GLFW_KEY_UP
	PageUp = 266, // GLFW_KEY_PAGE_UP
	PageDown = 267, // GLFW_KEY_PAGE_DOWN
	Home = 268, // GLFW_KEY_HOME
	End = 269, // GLFW_KEY_END
	CapsLock = 280, // GLFW_KEY_CAPS_LOCK
	ScrollLock = 281, // GLFW_KEY_SCROLL_LOCK
	NumLock = 282, // GLFW_KEY_NUM_LOCK
	PrintScreen = 283, // GLFW_KEY_PRINT_SCREEN
	Pause = 284, // GLFW_KEY_PAUSE
	F1 = 290, // GLFW_KEY_F1
	F2 = 291, // GLFW_KEY_F2
	F3 = 292, // GLFW_KEY_F3
	F4 = 293, // GLFW_KEY_F4
	F5 = 294, // GLFW_KEY_F5
	F6 = 295, // GLFW_KEY_F6
	F7 = 296, // GLFW_KEY_F7
	F8 = 297, // GLFW_KEY_F8
	F9 = 298, // GLFW_KEY_F9
	F10 = 299, // GLFW_KEY_F10
	F11 = 300, // GLFW_KEY_F11
	F12 = 301, // GLFW_KEY_F12
	F13 = 302, // GLFW_KEY_F13
	F14 = 303, // GLFW_KEY_F14
	F15 = 304, // GLFW_KEY_F15
	F16 = 305, // GLFW_KEY_F16
	F17 = 306, // GLFW_KEY_F17
	F18 = 307, // GLFW_KEY_F18
	F19 = 308, // GLFW_KEY_F19
	F20 = 309, // GLFW_KEY_F20
	F21 = 310, // GLFW_KEY_F21
	F22 = 311, // GLFW_KEY_F22
	F23 = 312, // GLFW_KEY_F23
	F24 = 313, // GLFW_KEY_F24
	F25 = 314, // GLFW_KEY_F25
	KP0 = 320, // GLFW_KEY_KP_0
	KP1 = 321, // GLFW_KEY_KP_1
	KP2 = 322, // GLFW_KEY_KP_2
	KP3 = 323, // GLFW_KEY_KP_3
	KP4 = 324, // GLFW_KEY_KP_4
	KP5 = 325, // GLFW_KEY_KP_5
	KP6 = 326, // GLFW_KEY_KP_6
	KP7 = 327, // GLFW_KEY_KP_7
	KP8 = 328, // GLFW_KEY_KP_8
	KP9 = 329, // GLFW_KEY_KP_9
	KPDecimal = 330, // GLFW_KEY_KP_DECIMAL
	KPDivide = 331, // GLFW_KEY_KP_DIVIDE
	KPMultiply = 332, // GLFW_KEY_KP_MULTIPLY
	KPSubtract = 333, // GLFW_KEY_KP_SUBTRACT
	KPAdd = 334, // GLFW_KEY_KP_ADD
	KPEnter = 335, // GLFW_KEY_KP_ENTER
	KPEqual = 336, // GLFW_KEY_KP_EQUAL
	LeftShift = 340, // GLFW_KEY_LEFT_SHIFT
	LeftControl = 341, // GLFW_KEY_LEFT_CONTROL
	LeftAlt = 342, // GLFW_KEY_LEFT_ALT
	LeftSuper = 343, // GLFW_KEY_LEFT_SUPER
	RightShift = 344, // GLFW_KEY_RIGHT_SHIFT
	RightControl = 345, // GLFW_KEY_RIGHT_CONTROL
	RightAlt = 346, // GLFW_KEY_RIGHT_ALT
	RightSuper = 347, // GLFW_KEY_RIGHT_SUPER
	Menu = 348, // GLFW_KEY_MENU
};

enum class KeyAction: int {
	Release = 0, // GLFW_RELEASE
	Press = 1, // GLFW_PRESS
	Repeat = 2, // GLFW_REPEAT
};

// Set the handler for keyboard inputs.
// Param 2 is KeyCode, param 4 is KeyAction
template<callable<void(Window, int, int, int, int)> Func>
void set_window_key_handler(Window window, Func&& func)
{
	detail::set_window_key_handler_raw(window, func);
}

// Set the handler for mouse cursor movement.
// Param 2 is x coordinate (in pixels), param 3 is y coordinate (in pixels)
template<callable<void(Window, double, double)> Func>
void set_window_cursor_motion_handler(Window window, Func&& func)
{
	detail::set_window_cursor_motion_handler_raw(window, func);
}

enum class MouseButton: int {
	Left = 0, // GLFW_MOUSE_BUTTON_LEFT
	Right = 1, // GLFW_MOUSE_BUTTON_RIGHT
	Middle = 2, // GLFW_MOUSE_BUTTON_MIDDLE
};

enum class MouseButtonAction: int {
	Release = 0, // GLFW_RELEASE
	Press = 1, // GLFW_PRESS
};

// Set the handler for mouse button inputs.
// Param 2 is MouseButton, param 3 is MouseButtonAction
template<callable<void(Window, int, int, int)> Func>
void set_window_mouse_button_handler(Window window, Func&& func)
{
	detail::set_window_mouse_button_handler_raw(window, func);
}

// Return the size of the window's framebuffer in pixels.
// Throws runtime_error on failure.
[[nodiscard]] auto get_window_framebuffer_size(Window window) -> uvec2;

// Return the scale of the display's content.
// Throws runtime_error on failure.
[[nodiscard]] auto get_window_content_scale(Window window) -> float;

// Create a Vulkan surface for the window.
// Throws runtime_error on failure.
[[nodiscard]] auto create_window_surface(Window window, vk::Instance instance) -> vk::Surface;

}
