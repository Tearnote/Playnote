#include "sys/window.hpp"

#include "libassert/assert.hpp"
#include "GLFW/glfw3.h"
#include "util/except.hpp"
#include "util/math.hpp"

auto Window::isClosing() -> bool
{
	return glfwWindowShouldClose(windowPtr.get());
}

void Window::poll()
{
	glfwPollEvents();
}

auto Window::size() -> uvec2
{
	auto w = 0;
	auto h = 0;
	glfwGetFramebufferSize(windowPtr.get(), &w, &h);
	return uvec2{static_cast<uint>(w), static_cast<uint>(h)};
}

auto Window::getCursorPosition() -> vec2
{
	auto x = 0.0;
	auto y = 0.0;
	glfwGetCursorPos(windowPtr.get(), &x, &y);
	return vec2{static_cast<float>(x), static_cast<float>(y)};
}

auto Window::getTime() -> nanoseconds
{
	auto time = std::chrono::duration<double>{glfwGetTime()};
	return duration_cast<nanoseconds>(time);
}

Window::Window(std::string const& title, uvec2 size)
{
	ASSUME(size.x() > 0 && size.y() > 0);

	// Create the window
	glfwSetErrorCallback(
		[](int code, char const* str) {
			throw runtime_error_fmt("[GLFW] Error {}: {}", code, str);
		}
	);
	glfwInit();
	L_INFO("GLFW initialized");

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
	registerKeyCallback(
		[this](int key, bool) {
			if (key == GLFW_KEY_ESCAPE)
				glfwSetWindowShouldClose(windowPtr.get(), GLFW_TRUE);
		}
	);

	L_INFO("Created window \"{}\" at {}x{}", title, size.x(), size.y());
}

Window::~Window()
{
	if (!windowPtr) return;

	windowPtr.reset();
	glfwTerminate();
	L_INFO("Window closed");
}
