/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/glfw.hpp"
#include "lib/vuk.hpp"

namespace playnote::lib::imgui {

// Bundle of resources used in Imgui integration. Opaque type.
struct Context_t;

namespace detail {
struct ContextDeleter {
	static void operator()(Context_t* ctx) noexcept;
};
}

using Context = unique_ptr<Context_t, detail::ContextDeleter>;

// Initialize Imgui and relevant GPU resources. font_data should be the contents of a TTF font.
// Throws if vuk throws.
auto init(glfw::Window window, vuk::Allocator& global_allocator, vector<byte>&& font_data) -> Context;

// Mark the start of a new frame for Imgui. All Imgui commands must come after this is called.
void begin();

// Finalize Imgui commands and prepare buffers for rendering. Imgui commands cannot be used anymore.
void end();

// Draw queued Imgui commands onto the provided image.
// Throws if vuk throws.
auto render(vuk::Allocator& frame_allocator, vuk::ManagedImage&& target, Context& context) -> vuk::ManagedImage;

// Start a new ImGui window. Initial position and size will be chosen automatically.
void begin_window(char const* title);

enum class WindowStyle {
	Normal,
	Static, // No title bar, no resizing, no moving
	Transparent, // No title bar, no resizing, no moving, no background
};

// Start a new ImGui window at a specific position and size. If static_frame is true, the window
// will have no title and won't be able to be modified by the user.
void begin_window(char const* title, int2 pos, int width, WindowStyle);

// Finalize a started window.
void end_window();

// Use before an element to keep it on the same line as the previous one.
void same_line();

// A clickable button. Returns true when clicked.
auto button(char const* str) -> bool;

// Static text.
void text(string_view str);

// Static text, fmt overload.
template<typename... Args>
void text(fmtquill::format_string<Args...> fmt, Args&&... args)
{
	text(format(fmt, forward<Args>(args)...).c_str());
}

enum class TextAlignment {
	Left,
	Center,
};

// Styled static text.
void text_styled(string_view str, optional<float4> color, float size = 1.0f, TextAlignment = TextAlignment::Left);

// A line of text that can be clicked like a button.
auto selectable(char const* str) -> bool;

// A control for a float variable, with +/- buttons and direct value input via keyboard.
void input_float(char const* str, float& value,
float step = 0.0f, float step_fast = 0.0f, char const* format = "%.3f");

// A control for a double variable, with +/- buttons and direct value input via keyboard.
void input_double(char const* str, double& value,
	double step = 0.0f, double step_fast = 0.0f, char const* format = "%.3f");

// A non-interactive progress bar control. If progress is nullopt, the bar will look intederminate.
void progress_bar(optional<float> progress, string_view text);

struct PlotValues {
	char const* name;
	span<float const> data;
	float4 color;
};

struct PlotMarker {
	enum Type {
		Horizontal,
		Vertical,
	};
	Type type;
	float value;
	float4 color;
};

// A simple line plot of an array of values.
void plot(char const* label, initializer_list<PlotValues> values, initializer_list<PlotMarker> markers = {}, int height = 0, bool stacked = false);

}
