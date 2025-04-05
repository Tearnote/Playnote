module;
#include <functional>
#include <utility>
#include <vector>
#include <memory>
#include "libassert/assert.hpp"
#include "GLFW/glfw3.h"
#include "util/log_macros.hpp"

export module playnote.sys.window;

import playnote.stx.except;
import playnote.stx.math;
import playnote.util.service;

namespace playnote::sys {
namespace chrono = std::chrono;
using chrono::duration_cast;
using stx::uvec2;
using stx::vec2;

// RAII abstraction for GLFW library initialization
class GLFW {
public:
	GLFW();
	~GLFW();

	[[nodiscard]] auto get_time() const -> chrono::nanoseconds;
	void poll() { glfwPollEvents(); }

	GLFW(GLFW const&) = delete;
	auto operator=(GLFW const&) -> GLFW& = delete;
	GLFW(GLFW&&) = delete;
	auto operator=(GLFW&&) -> GLFW& = delete;
};

// RAII abstraction of a single application window, providing a drawing surface and input handling
// Includes GLFW initialization, so only one can exist at a time
class Window {
public:
	Window(std::string const& title, uvec2 size);
	~Window();

	Window(Window const&) = delete;
	auto operator=(Window const&) -> Window& = delete;
	Window(Window&&) = delete;
	auto operator=(Window&&) -> Window& = delete;

	[[nodiscard]] auto is_closing() const -> bool { return glfwWindowShouldClose(windowPtr.get()); }
	[[nodiscard]] auto size() const -> uvec2;
	[[nodiscard]] auto get_cursor_position() const -> vec2;

	void register_key_callback(std::function<void(int, bool)> func)
	{
		keyCallbacks.emplace_back(std::move(func));
	}

	void register_cursor_motion_callback(std::function<void(vec2)> func)
	{
		cursorMotionCallbacks.emplace_back(std::move(func));
	}

	void register_mouse_button_callback(std::function<void(int, bool)> func)
	{
		mouseButtonCallbacks.emplace_back(std::move(func));
	}

	auto handle() -> GLFWwindow* { return windowPtr.get(); }

private:
	using GlfwWindowPtr = std::unique_ptr<GLFWwindow, decltype([](auto* w) {
		glfwDestroyWindow(w);
	})>;

	GlfwWindowPtr windowPtr;

	std::vector<std::function<void(int, bool)>> keyCallbacks;
	std::vector<std::function<void(vec2)>> cursorMotionCallbacks;
	std::vector<std::function<void(int, bool)>> mouseButtonCallbacks;
};

GLFW::GLFW()
{
	// No need to check GLFW functions' return codes with the error callback set
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

Window::Window(std::string const& title, uvec2 size)
{
	ASSUME(size.x() > 0 && size.y() > 0);

	// Create the window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	windowPtr =
		GlfwWindowPtr{glfwCreateWindow(size.x(), size.y(), title.c_str(), nullptr, nullptr)};
	ASSERT(windowPtr);

	// Set up event callbacks
	glfwSetWindowUserPointer(windowPtr.get(), this);

	glfwSetKeyCallback(windowPtr.get(),
		[](GLFWwindow* windowPtr, int key, int, int action, int) {
			if (action == GLFW_REPEAT) return;
			auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(windowPtr));
			for (auto& func: window.keyCallbacks)
				func(key, action == GLFW_PRESS);
		}
	);

	glfwSetCursorPosCallback(windowPtr.get(),
		[](GLFWwindow* windowPtr, double x, double y) {
			auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(windowPtr));
			for (auto& func: window.cursorMotionCallbacks)
				func(vec2(x, y));
		}
	);

	glfwSetMouseButtonCallback(windowPtr.get(),
		[](GLFWwindow* windowPtr, int button, int action, int) {
			auto& window = *static_cast<Window*>(glfwGetWindowUserPointer(windowPtr));
			for (auto& func: window.mouseButtonCallbacks)
				func(button, action == GLFW_PRESS);
		}
	);

	// Quit on ESC
	register_key_callback([this](int key, bool) {
		if (key == GLFW_KEY_ESCAPE)
			glfwSetWindowShouldClose(windowPtr.get(), GLFW_TRUE);
	});

	L_INFO("Created window \"{}\" at {}x{}", title, size.x(), size.y());
}

Window::~Window()
{
	if (!windowPtr) return;
	windowPtr.reset();
	L_INFO("Window closed");
}

auto Window::size() const -> uvec2
{
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(windowPtr.get(), &w, &h);
	return uvec2{static_cast<uint>(w), static_cast<uint>(h)};
}

auto Window::get_cursor_position() const -> vec2
{
	auto x = 0.0;
	auto y = 0.0;
	glfwGetCursorPos(windowPtr.get(), &x, &y);
	return vec2{static_cast<float>(x), static_cast<float>(y)};
}

export auto s_glfw = util::Service<GLFW>{};
export auto s_window = util::Service<Window>{};
}
