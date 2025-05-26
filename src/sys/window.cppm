/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/window.cppm:
Initializes GLFW and manages windows. Windows handle keyboard and mouse input, and the window's
"should close" state manages application lifetime.
*/

module;
#include "volk.h"
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.sys.window;

import playnote.lib.window;
import playnote.preamble;
import playnote.logger;

namespace playnote::sys {

// RAII abstraction for GLFW library initialization.
export class GLFW {
public:
	GLFW();
	~GLFW() noexcept;

	// Retrieve time since application start.
	[[nodiscard]] auto get_time() const noexcept -> nanoseconds { return lib::time_since_glfw_init(); }

	// Pump the message queue.
	// Run this as often as possible to receive accurate event timestamps.
	void poll() { lib::process_window_events(); }

	GLFW(GLFW const&) = delete;
	auto operator=(GLFW const&) -> GLFW& = delete;
	GLFW(GLFW&&) = delete;
	auto operator=(GLFW&&) -> GLFW& = delete;

private:
	static inline auto initialized = false;
};

// RAII abstraction of a single application window, providing a drawing surface and input handling.
export class Window {
public:
	using KeyCode = lib::KeyCode;
	using KeyAction = lib::KeyAction;
	using MouseButton = lib::MouseButton;
	using MouseButtonAction = lib::MouseButtonAction;

	Window(GLFW&, string_view title, uvec2 size);  // GLFW parameter is a semantic dependency

	// true if application close was requested (the X was pressed, or triggered manually from code
	// to mark a user-requested quit event).
	[[nodiscard]] auto is_closing() const noexcept -> bool
	{
		return lib::get_window_closing_flag(window_handle.get());
	}

	// Signal the application to cleanly close as soon as possible.
	void request_close() noexcept { lib::set_window_closing_flag(window_handle.get(), true); }

	// Size of the window's framebuffer.
	[[nodiscard]] auto size() const -> uvec2;

	// Run the provided function on any keyboard key press/release.
	// Function is provided with the keycode and key state (true for press, false for release).
	void register_key_callback(function<void(KeyCode, bool)>&& func) noexcept
	{
		key_callbacks.emplace_back(move(func));
	}

	// Run the provided function on any cursor move.
	// Function is provided with the new cursor position.
	void register_cursor_motion_callback(function<void(vec2)>&& func) noexcept
	{
		cursor_motion_callbacks.emplace_back(move(func));
	}

	// Run the provided function on any mouse button press/release.
	// Function is provided with the button index and state (true for press, false for release).
	void register_mouse_button_callback(function<void(MouseButton, bool)>&& func) noexcept
	{
		mouse_button_callbacks.emplace_back(move(func));
	}

	auto handle() noexcept -> lib::Window { return window_handle.get(); }

	// Create a Vulkan surface for the window's framebuffer.
	// Destruction needs to be handled manually by the caller.
	auto create_surface(VkInstance) -> VkSurfaceKHR;

	Window(Window const&) = delete;
	auto operator=(Window const&) -> Window& = delete;
	Window(Window&&) = delete;
	auto operator=(Window&&) -> Window& = delete;

private:
	using WindowHandle = unique_resource<lib::Window, decltype([](auto* w) noexcept {
		auto const title = lib::get_window_title(w);
		lib::destroy_window(w);
		INFO("Window \"{}\" closed", title);
	})>;

	WindowHandle window_handle{};

	vector<function<void(KeyCode, bool)>> key_callbacks;
	vector<function<void(vec2)>> cursor_motion_callbacks;
	vector<function<void(MouseButton, bool)>> mouse_button_callbacks;
};

GLFW::GLFW()
{
	if (initialized) throw runtime_error{"Attempted to initialize GLFW twice"};
	// Convert GLFW errors to exceptions, freeing us from having to check error codes
	lib::register_glfw_error_handler();
	lib::init_glfw();
	lib::set_window_creation_hints();
	initialized = true;
	INFO("GLFW initialized");
}

GLFW::~GLFW() noexcept
{
	if (!initialized) return;
	lib::cleanup_glfw();
	INFO("GLFW cleaned up");
}

Window::Window(GLFW&, string_view title, uvec2 size)
{
	ASSERT(size.x() > 0 && size.y() > 0);

	window_handle = WindowHandle{lib::create_window(size, title)};

	// Provide event callbacks full access to the associated Window instance
	lib::set_window_user_pointer(window_handle.get(), this);

	lib::set_key_handler(window_handle.get(), [](lib::Window window_ptr, int key, int, int action, int) {
		if (action == to_underlying(KeyAction::Repeat)) return; // Only care about press and release
		auto& window = *lib::get_window_user_pointer<Window>(window_ptr);
		for (auto& func: window.key_callbacks)
			func(KeyCode{key}, action == to_underlying(KeyAction::Press));
	});

	lib::set_cursor_motion_handler(window_handle.get(), [](lib::Window window_ptr, double x, double y) {
		auto& window = *lib::get_window_user_pointer<Window>(window_ptr);
		for (auto& func: window.cursor_motion_callbacks)
			func(vec2{static_cast<float>(x), static_cast<float>(y)});
	});

	lib::set_mouse_button_handler(window_handle.get(),
		[](lib::Window window_ptr, int button, int action, int) {
			auto& window = *lib::get_window_user_pointer<Window>(window_ptr);
			for (auto& func: window.mouse_button_callbacks)
				func(MouseButton{button}, action == to_underlying(MouseButtonAction::Press));
		}
	);

	INFO("Created window {}, size {}", title, size);
}

auto Window::size() const -> uvec2
{
	return lib::get_window_framebuffer_size(window_handle.get());
}

auto Window::create_surface(VkInstance instance) -> VkSurfaceKHR
{
	return lib::create_window_surface(window_handle.get(), instance);
}

}
