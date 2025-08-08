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

enum class KeyCode: int {};

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

// Create a Vulkan surface for the window.
// Throws runtime_error on failure.
[[nodiscard]] auto create_window_surface(Window window, vk::Instance const& instance) -> vk::Surface;

}
