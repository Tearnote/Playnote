/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/glfw.cpp:
Implementation file for lib/glfw.hpp.
*/

#include "lib/glfw.hpp"

#include <vulkan/vulkan_core.h>
#include "GLFW/glfw3.h"
#include "preamble.hpp"
#include "assert.hpp"
#include "lib/vulkan.hpp"

namespace playnote::lib::glfw {

namespace detail {

void set_window_user_pointer_raw(Window window, void* ptr)
{
	ASSERT(window);
	glfwSetWindowUserPointer(window, ptr);
}

[[nodiscard]] auto get_window_user_pointer_raw(Window window) -> void*
{
	ASSERT(window);
	return glfwGetWindowUserPointer(window);
}

void set_window_key_handler_raw(Window window, void (*func)(Window, int, int, int, int))
{
	ASSERT(window);
	glfwSetKeyCallback(window, func);
}

void set_window_cursor_motion_handler_raw(Window window, void (*func)(Window, double, double))
{
	ASSERT(window);
	glfwSetCursorPosCallback(window, func);
}

void set_window_mouse_button_handler_raw(Window window, void (*func)(Window, int, int, int))
{
	ASSERT(window);
	glfwSetMouseButtonCallback(window, func);
}

}

void register_error_handler()
{
	glfwSetErrorCallback([](int code, char const* str) {
		throw runtime_error_fmt("[GLFW] Error #{}: {}", code, str);
	});
}

void init() { glfwInit(); }

void cleanup() noexcept try { glfwTerminate(); }
catch (runtime_error const&) {}

[[nodiscard]] auto time_since_init() -> nanoseconds
try {
	auto const time = duration<double>{glfwGetTime()};
	return duration_cast<nanoseconds>(time);
}
catch (runtime_error const&) {
	return nanoseconds{0};
}

void process_events() { glfwPollEvents(); }

[[nodiscard]] auto get_window_closing_flag(Window window) -> bool
{
	ASSERT(window);
	auto const result = glfwWindowShouldClose(window);
	ASSUME(result == 0 || result == 1);
	return result;
}

void set_window_closing_flag(Window window, bool flag_value)
{
	ASSERT(window);
	glfwSetWindowShouldClose(window, flag_value);
}

void set_window_creation_hints()
try {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
}
catch (runtime_error const&) {
	ASSERT(false);
}

auto create_window(uvec2 size, string_view title) -> Window
{
	return ASSUME_VAL(glfwCreateWindow(size.x(), size.y(), string{title}.c_str(), nullptr, nullptr));
}

void destroy_window(Window window) noexcept
try {
	if (!window) return;
	glfwDestroyWindow(window);
}
catch (runtime_error const&) {}

[[nodiscard]] auto get_window_title(Window window) -> string_view
{
	ASSERT(window);
	return glfwGetWindowTitle(window);
}

[[nodiscard]] auto get_window_framebuffer_size(Window window) -> uvec2
{
	ASSERT(window);
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(window, &w, &h);
	return uvec2{static_cast<uint32>(w), static_cast<uint32>(h)};
}

auto get_window_content_scale(Window window) -> float
{
	auto xscale = 0.0f;
	auto yscale = 0.0f;
	glfwGetWindowContentScale(window, &xscale, &yscale);
	ASSERT(xscale == yscale);
	ASSERT(xscale > 0.0f);
	return xscale;
}

[[nodiscard]] auto create_window_surface(Window window, vk::Instance instance) -> vk::Surface
{
	auto result = VkSurfaceKHR{nullptr};
	glfwCreateWindowSurface(vk::get_raw_instance(instance), window, nullptr, &result);
	return result;
}

}
