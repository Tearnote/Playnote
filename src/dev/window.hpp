/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/service.hpp"
#include "utils/assert.hpp"
#include "utils/logger.hpp"
#include "lib/vulkan.hpp"
#include "lib/glfw.hpp"

namespace playnote::dev {

// RAII abstraction for GLFW library initialization.
class GLFW {
public:
	GLFW();
	~GLFW() noexcept;

	// Retrieve time since application start.
	[[nodiscard]] auto get_time() const -> nanoseconds { return lib::glfw::time_since_init(); }

	// Pump the message queue.
	// Run this as often as possible to receive accurate event timestamps.
	void poll() { lib::glfw::process_events(); }

	GLFW(GLFW const&) = delete;
	auto operator=(GLFW const&) -> GLFW& = delete;
	GLFW(GLFW&&) = delete;
	auto operator=(GLFW&&) -> GLFW& = delete;

private:
	InstanceLimit<GLFW, 1> instance_limit;
};

// RAII abstraction of a single application window, providing a drawing surface and input handling.
class Window {
public:
	using KeyCode = lib::glfw::KeyCode;
	using KeyAction = lib::glfw::Action;
	using MouseButton = lib::glfw::MouseButton;
	using MouseButtonAction = lib::glfw::MouseButtonAction;

	Window(string_view title, int2 size);

	// true if application close was requested (the X was pressed, or triggered manually from code
	// to mark a user-requested quit event).
	[[nodiscard]] auto is_closing() const -> bool
	{
		return lib::glfw::get_window_closing_flag(window_handle.get());
	}

	// Signal the application to cleanly close as soon as possible.
	void request_close() { lib::glfw::set_window_closing_flag(window_handle.get(), true); }

	// Size of the window's framebuffer.
	[[nodiscard]] auto size() const -> int2;

	// Scale factor of the window, useful for converting pixel coordinates.
	[[nodiscard]] auto scale() const -> float;

	// Run the provided function on any keyboard key press/release.
	// Function is provided with the keycode and key state (true for press, false for release).
	void register_key_callback(function<void(KeyCode, bool)>&& func)
	{
		key_callbacks.emplace_back(move(func));
	}

	// Run the provided function on any cursor move.
	// Function is provided with the new cursor position.
	void register_cursor_motion_callback(function<void(float2)>&& func)
	{
		cursor_motion_callbacks.emplace_back(move(func));
	}

	// Run the provided function on any mouse button press/release.
	// Function is provided with the button index and state (true for press, false for release).
	void register_mouse_button_callback(function<void(MouseButton, bool)>&& func)
	{
		mouse_button_callbacks.emplace_back(move(func));
	}

	// Run the provided function when the user drops files onto the window.
	// Function is provided with the list of paths dropped.
	void register_file_drop_callback(function<void(span<char const* const>)>&& func)
	{
		file_drop_callbacks.emplace_back(move(func));
	}

	auto handle() -> lib::glfw::Window { return window_handle.get(); }

	// Create a Vulkan surface for the window's framebuffer.
	// Destruction needs to be handled manually by the caller.
	auto create_surface(lib::vk::Instance const&) -> lib::vk::Surface;

	auto cursor_position() -> float2;

	Window(Window const&) = delete;
	auto operator=(Window const&) -> Window& = delete;
	Window(Window&&) = delete;
	auto operator=(Window&&) -> Window& = delete;

private:
	using WindowHandle = unique_resource<lib::glfw::Window, decltype([](auto* w) noexcept {
		lib::glfw::destroy_window(w);
		INFO("Window closed");
	})>;

	WindowHandle window_handle{};
	vector<function<void(KeyCode, bool)>> key_callbacks;
	vector<function<void(float2)>> cursor_motion_callbacks;
	vector<function<void(MouseButton, bool)>> mouse_button_callbacks;
	vector<function<void(span<char const* const>)>> file_drop_callbacks;
};

inline GLFW::GLFW()
{
	// Convert GLFW errors to exceptions, freeing us from having to check error codes
	lib::glfw::register_error_handler();
	lib::glfw::init();
	lib::glfw::set_window_creation_hints();
	INFO("GLFW initialized");
}

inline GLFW::~GLFW() noexcept
{
	lib::glfw::cleanup();
	INFO("GLFW cleaned up");
}

inline Window::Window(string_view title, int2 size) {
	ASSERT(size.x() > 0 && size.y() > 0);

	window_handle = WindowHandle{lib::glfw::create_window(size, title)};

	// Provide event callbacks full access to the associated Window instance
	lib::glfw::set_window_user_pointer(window_handle.get(), this);

	lib::glfw::set_window_key_handler(window_handle.get(), [](lib::glfw::Window window_ptr, int key, int, int action, int) {
		if (action == +KeyAction::Repeat) return; // Only care about press and release
		auto& window = *lib::glfw::get_window_user_pointer<Window>(window_ptr);
		for (auto& func: window.key_callbacks)
			func(KeyCode{key}, action == +KeyAction::Press);
	});

	lib::glfw::set_window_cursor_motion_handler(window_handle.get(), [](lib::glfw::Window window_ptr, double x, double y) {
		auto& window = *lib::glfw::get_window_user_pointer<Window>(window_ptr);
		for (auto& func: window.cursor_motion_callbacks)
			func(float2{static_cast<float>(x), static_cast<float>(y)});
	});

	lib::glfw::set_window_mouse_button_handler(window_handle.get(),
		[](lib::glfw::Window window_ptr, int button, int action, int) {
			auto& window = *lib::glfw::get_window_user_pointer<Window>(window_ptr);
			for (auto& func: window.mouse_button_callbacks)
				func(MouseButton{button}, action == +MouseButtonAction::Press);
		}
	);

	lib::glfw::set_window_file_drop_handler(window_handle.get(),
		[](lib::glfw::Window window_ptr, int count, char const** paths_raw) {
			auto& window = *lib::glfw::get_window_user_pointer<Window>(window_ptr);
			auto const paths = span{paths_raw, static_cast<size_t>(count)};
			for (auto& func: window.file_drop_callbacks)
				func(paths);
		}
	);

	INFO("Created window {}, size {}", title, size);
}

inline auto Window::size() const -> int2
{
	return lib::glfw::get_window_framebuffer_size(window_handle.get());
}

inline auto Window::scale() const -> float
{
	return lib::glfw::get_window_content_scale(window_handle.get());
}

inline auto Window::create_surface(lib::vk::Instance const& instance) -> lib::vk::Surface
{
	return lib::glfw::create_window_surface(window_handle.get(), instance);
}

inline auto Window::cursor_position() -> float2
{
	return lib::glfw::get_window_cursor_position(window_handle.get());
}

}

namespace playnote::globals {
inline auto glfw = Service<dev::GLFW>{};
}
