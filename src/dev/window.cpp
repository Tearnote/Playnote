/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "dev/window.hpp"

#include "preamble.hpp"
#include "utils/assert.hpp"

namespace playnote::dev {

GLFW::GLFW()
{
	// Convert GLFW errors to exceptions, freeing us from having to check error codes
	lib::glfw::register_error_handler();
	lib::glfw::init();
	lib::glfw::set_window_creation_hints();
	INFO("GLFW initialized");
}

GLFW::~GLFW() noexcept
{
	lib::glfw::cleanup();
	INFO("GLFW cleaned up");
}

Window::Window(string_view title, int2 size) {
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

auto Window::size() const -> int2
{ return lib::glfw::get_window_framebuffer_size(window_handle.get()); }

auto Window::scale() const -> float
{ return lib::glfw::get_window_content_scale(window_handle.get()); }

auto Window::create_surface(lib::vk::Instance const& instance) -> lib::vk::Surface
{ return lib::glfw::create_window_surface(window_handle.get(), instance); }

auto Window::cursor_position() -> float2
{ return lib::glfw::get_window_cursor_position(window_handle.get()); }

}
