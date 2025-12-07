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
	{ return lib::glfw::get_window_closing_flag(window_handle.get()); }

	// Signal the application to cleanly close as soon as possible.
	void request_close() { lib::glfw::set_window_closing_flag(window_handle.get(), true); }

	// Size of the window's framebuffer.
	[[nodiscard]] auto size() const -> int2;

	// Scale factor of the window, useful for converting pixel coordinates.
	[[nodiscard]] auto scale() const -> float;

	// Run the provided function on any keyboard key press/release.
	// Function is provided with the keycode and key state (true for press, false for release).
	void register_key_callback(function<void(KeyCode, bool)> func)
	{ key_callbacks.emplace_back(move(func)); }

	// Run the provided function on any cursor move.
	// Function is provided with the new cursor position.
	void register_cursor_motion_callback(function<void(float2)> func)
	{ cursor_motion_callbacks.emplace_back(move(func)); }

	// Run the provided function on any mouse button press/release.
	// Function is provided with the button index and state (true for press, false for release).
	void register_mouse_button_callback(function<void(MouseButton, bool)> func)
	{ mouse_button_callbacks.emplace_back(move(func)); }

	// Run the provided function when the user drops files onto the window.
	// Function is provided with the list of paths dropped.
	void register_file_drop_callback(function<void(span<char const* const>)> func)
	{ file_drop_callbacks.emplace_back(move(func)); }

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

}

namespace playnote::globals {
inline auto glfw = Service<dev::GLFW>{};
}
