module;
#include <string_view>
#include <functional>
#include <utility>
#include <vector>
#include <string>
#include <vulkan/vulkan_core.h>

#include "libassert/assert.hpp"
#include "volk.h"
#include "GLFW/glfw3.h"
#include "util/log_macros.hpp"

export module playnote.sys.window;

import playnote.stx.except;
import playnote.stx.math;
import playnote.util.raii;
import playnote.globals;

namespace playnote::sys {
namespace chrono = std::chrono;
using chrono::duration_cast;
using stx::uvec2;
using stx::vec2;

// RAII abstraction for GLFW library initialization
export class GLFW {
public:
	GLFW();
	~GLFW();

	// Retrieve time since application start
	[[nodiscard]] auto get_time() const -> chrono::nanoseconds;

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
	Window(GLFW&, std::string_view title, uvec2 size);  // GLFW parameter is a semantic dependency

	// true if application close was requested (the X was pressed, or triggered manually from code
	// to mark a user-requested quit event)
	[[nodiscard]] auto is_closing() const -> bool { return glfwWindowShouldClose(*window_handle); }

	// Size of the window's framebuffer
	[[nodiscard]] auto size() const -> uvec2;

	// Position of the cursor relative to the window's framebuffer
	[[nodiscard]] auto get_cursor_position() const -> vec2;

	// Run the provided function on any keyboard key press/release
	// Function is provided with the keycode and key state (true for press, false for release)
	void register_key_callback(std::function<void(int, bool)> func)
	{
		key_callbacks.emplace_back(std::move(func));
	}

	// Run the provided function on any cursor move
	// Function is provided with the new cursor position
	void register_cursor_motion_callback(std::function<void(vec2)> func)
	{
		cursor_motion_callbacks.emplace_back(std::move(func));
	}

	// Run the provided function on any mouse button press/release
	// Function is provided with the button index and state (true for press, false for release)
	void register_mouse_button_callback(std::function<void(int, bool)> func)
	{
		mouse_button_callbacks.emplace_back(std::move(func));
	}

	auto handle() -> GLFWwindow* { return *window_handle; }

	// Create a Vulkan surface for the window's framebuffer
	// Destruction needs to be handled manually by the caller
	auto create_surface(VkInstance) -> VkSurfaceKHR;

	Window(Window const&) = delete;
	auto operator=(Window const&) -> Window& = delete;
	Window(Window&&) = delete;
	auto operator=(Window&&) -> Window& = delete;

private:
	using WindowHandle = util::RAIIResource<GLFWwindow*, decltype([](auto* w) {
		auto title = std::string{glfwGetWindowTitle(w)};
		glfwDestroyWindow(w);
		L_INFO("Window \"{}\" closed", title);
	})>;

	WindowHandle window_handle{};

	std::vector<std::function<void(int, bool)>> key_callbacks;
	std::vector<std::function<void(vec2)>> cursor_motion_callbacks;
	std::vector<std::function<void(int, bool)>> mouse_button_callbacks;
};

GLFW::GLFW()
{
	// Convert GLFW errors to exceptions, freeing us from having to check error codes
	glfwSetErrorCallback([](int code, char const* str) {
		throw stx::runtime_error_fmt("[GLFW] Error {}: {}", code, str);
	});
	glfwInit();
	L_INFO("GLFW initialized");
}

GLFW::~GLFW()
{
	glfwTerminate();
	L_INFO("GLFW cleaned up");
}

auto GLFW::get_time() const -> chrono::nanoseconds
{
	auto time = std::chrono::duration<double>{glfwGetTime()};
	return duration_cast<chrono::nanoseconds>(time);
}

Window::Window(GLFW&, std::string_view title, uvec2 size)
{
	ASSUME(size.x() > 0 && size.y() > 0);

	// Create the window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window_handle = WindowHandle{
		glfwCreateWindow(size.x(), size.y(), std::string{title}.c_str(), nullptr, nullptr)
	};
	ASSERT(*window_handle);

	// Provide event callbacks full access to the associated Window instance
	glfwSetWindowUserPointer(*window_handle, this);

	glfwSetKeyCallback(*window_handle, [](GLFWwindow* window_ptr, int key, int, int action, int) {
		if (action == GLFW_REPEAT) return; // Only care about press and release
		auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(window_ptr));
		for (auto& func: window.key_callbacks)
			func(key, action == GLFW_PRESS);
	});

	glfwSetCursorPosCallback(*window_handle, [](GLFWwindow* window_ptr, double x, double y) {
		auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(window_ptr));
		for (auto& func: window.cursor_motion_callbacks)
			func(vec2{static_cast<float>(x), static_cast<float>(y)});
	});

	glfwSetMouseButtonCallback(*window_handle,
		[](GLFWwindow* window_ptr, int button, int action, int) {
			auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(window_ptr));
			for (auto& func: window.mouse_button_callbacks)
				func(button, action == GLFW_PRESS);
		}
	);

	L_INFO("Created window \"{}\" at {}x{}", title, size.x(), size.y());
}

auto Window::size() const -> uvec2
{
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(*window_handle, &w, &h);
	return uvec2{static_cast<uint>(w), static_cast<uint>(h)};
}

auto Window::get_cursor_position() const -> vec2
{
	auto x = 0.0;
	auto y = 0.0;
	glfwGetCursorPos(*window_handle, &x, &y);
	return vec2{static_cast<float>(x), static_cast<float>(y)};
}

auto Window::create_surface(VkInstance instance) -> VkSurfaceKHR
{
	auto result = VkSurfaceKHR{nullptr};
	glfwCreateWindowSurface(instance, *window_handle, nullptr, &result);
	return result;
}

}
