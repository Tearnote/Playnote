/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/window.cppm:
Wrapper for GLFW window and input handling.
*/

module;
#include <vulkan/vulkan_core.h>
#include "GLFW/glfw3.h"
#include "volk.h"
#include "macros/assert.hpp"

export module playnote.lib.window;

import playnote.preamble;

namespace playnote::lib {

// Opaque handle to a window.
export using Window = GLFWwindow*;

// Make GLFW throw on any error. Can be called anytime.
export void register_glfw_error_handler() noexcept
{
	glfwSetErrorCallback([](int code, char const* str) {
		throw runtime_error_fmt("[GLFW] Error #{}: {}", code, str);
	});
}

// Initialize GLFW.
// Throws runtime_error on failure.
export void init_glfw() { glfwInit(); }

// Clean up GLFW.
// Errors are ignored.
export void cleanup_glfw() noexcept try { glfwTerminate(); }
catch (runtime_error const&) {}

// Return the me passed since the call to init_glfw(). If GLFW is not initialized, returns 0.
export [[nodiscard]] auto time_since_glfw_init() noexcept -> nanoseconds try
{
	auto const time = duration<double>{glfwGetTime()};
	return duration_cast<nanoseconds>(time);
}
catch (runtime_error const&) {
	return nanoseconds{0};
}

// Check for all open windows' events, and dispatch registered callbacks.
// Throws runtime_error on failure, or if a callback throws.
export void process_window_events() { glfwPollEvents(); }

// Return the current value of the window's "closing" flag. This flag is set automatically
// if the user presses the "X" in the corner, or programmatically via set_window_closing_flag.
export [[nodiscard]] auto get_window_closing_flag(Window window) noexcept -> bool
{
	ASSERT(window);
	auto const result = glfwWindowShouldClose(window);
	ASSUME(result == 0 || result == 1);
	return result;
}

// Set the current value of the window's "closing" flag.
export void set_window_closing_flag(Window window, bool flag_value) noexcept
{
	ASSERT(window);
	glfwSetWindowShouldClose(window, flag_value);
}

// Set window creation hints to the expected values.
export void set_window_creation_hints() noexcept try
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
}
catch (runtime_error const&) {
	ASSERT(false);
}

// Open a new window and return the handle. Close it once done.
// Throws runtime_error on failure.
export auto create_window(uvec2 size, string_view title) -> Window
{
	return ASSUME_VAL(glfwCreateWindow(size.x(), size.y(), string{title}.c_str(), nullptr, nullptr));
}

// Destroy a previously opened window.
// Errors are ignored.
export void destroy_window(Window window) noexcept try
{
	if (!window) return;
	glfwDestroyWindow(window);
}
catch (runtime_error const&) {}

// Set the custom pointer value that gets passed to event handlers.
export template<typename T>
void set_window_user_pointer(Window window, T* ptr) noexcept
{
	ASSERT(window);
	glfwSetWindowUserPointer(window, ptr);
}

export template<typename T>
[[nodiscard]] auto get_window_user_pointer(Window window) noexcept -> T*
{
	ASSERT(window);
	return static_cast<T*>(glfwGetWindowUserPointer(window));
}

// Return the Window's title. String is valid until it's changed by another call, or the window
// is destroyed.
export [[nodiscard]] auto get_window_title(Window window) noexcept -> string_view
{
	ASSERT(window);
	return glfwGetWindowTitle(window);
}

export enum class KeyCode: int {};

export enum class KeyAction: int {
	Release = GLFW_RELEASE,
	Press = GLFW_PRESS,
	Repeat = GLFW_REPEAT,
};

// Set the handler for keyboard inputs.
// Param 2 is KeyCode, param 4 is KeyAction
export template<callable<void(Window, int, int, int, int)> Func>
void set_key_handler(Window window, Func&& func) noexcept
{
	ASSERT(window);
	glfwSetKeyCallback(window, func);
}

// Set the handler for mouse cursor movement.
// Param 2 is x coordinate (in pixels), param 3 is y coordinate (in pixels)
export template<callable<void(Window, double, double)> Func>
void set_cursor_motion_handler(Window window, Func&& func) noexcept
{
	ASSERT(window);
	glfwSetCursorPosCallback(window, func);
}

export enum class MouseButton: int {
	Left = GLFW_MOUSE_BUTTON_LEFT,
	Right = GLFW_MOUSE_BUTTON_RIGHT,
	Middle = GLFW_MOUSE_BUTTON_MIDDLE,
};

export enum class MouseButtonAction: int {
	Release = GLFW_RELEASE,
	Press = GLFW_PRESS,
};

// Set the handler for mouse button inputs.
// Param 2 is MouseButton, param 3 is MouseButtonAction
export template<callable<void(Window, int, int, int)> Func>
void set_mouse_button_handler(Window window, Func&& func) noexcept
{
	ASSERT(window);
	glfwSetMouseButtonCallback(window, func);
}

// Return the size of the window's framebuffer in pixels.
// Throws runtime_error on failure.
export [[nodiscard]] auto get_window_framebuffer_size(Window window) -> uvec2
{
	ASSERT(window);
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(window, &w, &h);
	return uvec2{static_cast<uint>(w), static_cast<uint>(h)};
}

// Create a Vulkan surface for the window.
// Throws runtime_error on failure.
export [[nodiscard]] auto create_window_surface(Window window, VkInstance instance) -> VkSurfaceKHR
{
	auto result = VkSurfaceKHR{nullptr};
	glfwCreateWindowSurface(instance, window, nullptr, &result);
	return result;
}

}
