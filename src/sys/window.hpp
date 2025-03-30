#pragma once

#include <functional>
#include <utility>
#include <vector>
#include <memory>
#include "GLFW/glfw3.h"
#include "util/logger.hpp"
#include "util/math.hpp"
#include "util/time.hpp"

class Window_impl:
	Logger
{
public:
	auto isClosing() -> bool;

	void poll();

	auto size() -> uvec2;

	auto getCursorPosition() -> vec2;

	void registerKeyCallback(std::function<void(int, bool)> func)
	{
		keyCallbacks.emplace_back(std::move(func));
	}

	void registerCursorMotionCallback(std::function<void(vec2)> func)
	{
		cursorMotionCallbacks.emplace_back(std::move(func));
	}

	void registerMouseButtonCallback(std::function<void(int, bool)> func)
	{
		mouseButtonCallbacks.emplace_back(std::move(func));
	}

	auto getTime() -> nanoseconds;

	auto handle() -> GLFWwindow* { return windowPtr.get(); }

private:
	// Only usable from the service
	friend class Service<Window_impl>;

	Window_impl(std::string const& title, uvec2 size);

	~Window_impl();

	// Moveable
	Window_impl(Window_impl&&) = default;
	auto operator=(Window_impl&&) -> Window_impl& = default;

	using GlfwWindowPtr = std::unique_ptr<GLFWwindow, decltype(
		[](auto* w)
		{
			glfwDestroyWindow(w);
		}
	)>;

	GlfwWindowPtr windowPtr;

	std::vector<std::function<void(int, bool)>> keyCallbacks;
	std::vector<std::function<void(vec2)>> cursorMotionCallbacks;
	std::vector<std::function<void(int, bool)>> mouseButtonCallbacks;
};

using Window = Service<Window_impl>;
