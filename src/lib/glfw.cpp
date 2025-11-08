/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/glfw.hpp"

#include <vulkan/vulkan_core.h>
#include "GLFW/glfw3.h"
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "lib/vulkan.hpp"

namespace playnote::lib::glfw {

void detail::set_window_user_pointer_raw(Window window, void* ptr)
{
	ASSERT(window);
	glfwSetWindowUserPointer(window, ptr);
}

[[nodiscard]] auto detail::get_window_user_pointer_raw(Window window) -> void*
{
	ASSERT(window);
	return glfwGetWindowUserPointer(window);
}

void detail::set_window_key_handler_raw(Window window, void (*func)(Window, int, int, int, int))
{
	ASSERT(window);
	glfwSetKeyCallback(window, func);
}

void detail::set_window_cursor_motion_handler_raw(Window window, void (*func)(Window, double, double))
{
	ASSERT(window);
	glfwSetCursorPosCallback(window, func);
}

void detail::set_window_mouse_button_handler_raw(Window window, void (*func)(Window, int, int, int))
{
	ASSERT(window);
	glfwSetMouseButtonCallback(window, func);
}

void detail::set_window_file_drop_handler_raw(Window window, void(*func)(Window, int, char const**))
{
	ASSERT(window);
	glfwSetDropCallback(window, func);
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

auto create_window(int2 size, string_view title) -> Window
{
	return ASSUME_VAL(glfwCreateWindow(size.x(), size.y(), string{title}.c_str(), nullptr, nullptr));
}

void destroy_window(Window window) noexcept
try {
	if (!window) return;
	glfwDestroyWindow(window);
}
catch (runtime_error const&) {}

[[nodiscard]] auto get_window_framebuffer_size(Window window) -> int2
{
	ASSERT(window);
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(window, &w, &h);
	return int2{w, h};
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

[[nodiscard]] auto get_window_cursor_position(Window window) -> float2
{
	auto xpos = 0.0;
	auto ypos = 0.0;
	glfwGetCursorPos(window, &xpos, &ypos);
	return float2{static_cast<float>(xpos), static_cast<float>(ypos)} * get_window_content_scale(window);
}

}
