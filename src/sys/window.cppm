/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/window.cppm:
Initializes GLFW and manages windows. Windows handle keyboard and mouse input, and the window's
"should close" state manages application lifetime.
*/

module;
#include <vulkan/vulkan_core.h>
#include "libassert/assert.hpp"
#include "volk.h"
#include "GLFW/glfw3.h"
#include "util/logger.hpp"

export module playnote.sys.window;

import playnote.preamble;
import playnote.util.logger;

namespace playnote::sys {

// RAII abstraction for GLFW library initialization
export class GLFW {
public:
	GLFW();
	~GLFW();

	// Retrieve time since application start
	[[nodiscard]] auto get_time() const -> nanoseconds;

	// Pump the message queue
	// Run this as often as possible to receive accurate event timestamps
	void poll() { glfwPollEvents(); }

	GLFW(GLFW const&) = delete;
	auto operator=(GLFW const&) -> GLFW& = delete;
	GLFW(GLFW&&) = delete;
	auto operator=(GLFW&&) -> GLFW& = delete;
};

// RAII abstraction of a single application window, providing a drawing surface and input handling
export class Window {
public:
	Window(GLFW&, string const& title, uvec2 size);  // GLFW parameter is a semantic dependency

	// true if application close was requested (the X was pressed, or triggered manually from code
	// to mark a user-requested quit event)
	[[nodiscard]] auto is_closing() const -> bool { return glfwWindowShouldClose(window_handle.get()); }

	// Signal the application to cleanly close as soon as possible
	void request_close() { glfwSetWindowShouldClose(window_handle.get(), true); }

	// Size of the window's framebuffer
	[[nodiscard]] auto size() const -> uvec2;

	// Position of the cursor relative to the window's framebuffer
	[[nodiscard]] auto get_cursor_position() const -> vec2;

	// Run the provided function on any keyboard key press/release
	// Function is provided with the keycode and key state (true for press, false for release)
	void register_key_callback(function<void(int, bool)> func)
	{
		key_callbacks.emplace_back(move(func));
	}

	// Run the provided function on any cursor move
	// Function is provided with the new cursor position
	void register_cursor_motion_callback(function<void(vec2)> func)
	{
		cursor_motion_callbacks.emplace_back(move(func));
	}

	// Run the provided function on any mouse button press/release
	// Function is provided with the button index and state (true for press, false for release)
	void register_mouse_button_callback(function<void(int, bool)> func)
	{
		mouse_button_callbacks.emplace_back(move(func));
	}

	auto handle() -> GLFWwindow* { return window_handle.get(); }

	// Create a Vulkan surface for the window's framebuffer
	// Destruction needs to be handled manually by the caller
	auto create_surface(VkInstance) -> VkSurfaceKHR;

	Window(Window const&) = delete;
	auto operator=(Window const&) -> Window& = delete;
	Window(Window&&) = delete;
	auto operator=(Window&&) -> Window& = delete;

private:
	using WindowHandle = unique_resource<GLFWwindow*, decltype([](auto* w) {
		auto title = string{glfwGetWindowTitle(w)};
		glfwDestroyWindow(w);
		INFO("Window \"{}\" closed", title);
	})>;

	WindowHandle window_handle{};

	vector<function<void(int, bool)>> key_callbacks;
	vector<function<void(vec2)>> cursor_motion_callbacks;
	vector<function<void(int, bool)>> mouse_button_callbacks;
};

GLFW::GLFW()
{
	// Convert GLFW errors to exceptions, freeing us from having to check error codes
	glfwSetErrorCallback([](int code, char const* str) {
		throw runtime_error_fmt("[GLFW] Error {}: {}", code, str);
	});
	glfwInit();
	INFO("GLFW initialized");
}

GLFW::~GLFW()
{
	glfwTerminate();
	INFO("GLFW cleaned up");
}

auto GLFW::get_time() const -> nanoseconds
{
	auto time = duration<double>{glfwGetTime()};
	return duration_cast<nanoseconds>(time);
}

Window::Window(GLFW&, string const& title, uvec2 size)
{
	ASSUME(size.x() > 0 && size.y() > 0);

	// Create the window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window_handle = WindowHandle{
		glfwCreateWindow(size.x(), size.y(), title.c_str(), nullptr, nullptr)
	};
	ASSERT(window_handle.get());

	// Provide event callbacks full access to the associated Window instance
	glfwSetWindowUserPointer(window_handle.get(), this);

	glfwSetKeyCallback(window_handle.get(), [](GLFWwindow* window_ptr, int key, int, int action, int) {
		if (action == GLFW_REPEAT) return; // Only care about press and release
		auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(window_ptr));
		for (auto& func: window.key_callbacks)
			func(key, action == GLFW_PRESS);
	});

	glfwSetCursorPosCallback(window_handle.get(), [](GLFWwindow* window_ptr, double x, double y) {
		auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(window_ptr));
		for (auto& func: window.cursor_motion_callbacks)
			func(vec2{static_cast<float>(x), static_cast<float>(y)});
	});

	glfwSetMouseButtonCallback(window_handle.get(),
		[](GLFWwindow* window_ptr, int button, int action, int) {
			auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(window_ptr));
			for (auto& func: window.mouse_button_callbacks)
				func(button, action == GLFW_PRESS);
		}
	);

	INFO("Created window \"{}\" at {}x{}", title, size.x(), size.y());
}

auto Window::size() const -> uvec2
{
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(window_handle.get(), &w, &h);
	return uvec2{static_cast<uint>(w), static_cast<uint>(h)};
}

auto Window::get_cursor_position() const -> vec2
{
	auto x = 0.0;
	auto y = 0.0;
	glfwGetCursorPos(window_handle.get(), &x, &y);
	return vec2{static_cast<float>(x), static_cast<float>(y)};
}

auto Window::create_surface(VkInstance instance) -> VkSurfaceKHR
{
	auto result = VkSurfaceKHR{nullptr};
	glfwCreateWindowSurface(instance, window_handle.get(), nullptr, &result);
	return result;
}

}
