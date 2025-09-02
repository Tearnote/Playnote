/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/imgui.hpp:
Imgui initialization and usage.
Parts are adapted from vuk's built-in Imgui integration; reproduced here under the MIT license.
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

// Initialize Imgui and relevant GPU resources.
// Throws if vuk throws.
auto init(glfw::Window window, vuk::Allocator& global_allocator) -> Context;

// Mark the start of a new frame for Imgui. All Imgui commands must come after this is called.
void begin();

// Finalize Imgui commands and prepare buffers for rendering. Imgui commands cannot be used anymore.
void end();

// Draw queued Imgui commands onto the provided image.
// Throws if vuk throws.
auto render(vuk::Allocator& frame_allocator, vuk::ManagedImage&& target, Context& context) -> vuk::ManagedImage;

// Start a new ImGui window. Initial position and size will be chosen automatically.
void begin_window(char const* title);

// Start a new ImGui window at a specific position and size. If static_frame is true, the window
// will have no title and won't be able to be modified by the user.
void begin_window(char const* title, uvec2 pos, uint32 width, bool static_frame = false);

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
	vec4 color;
};

struct PlotMarker {
	enum Type {
		Horizontal,
		Vertical,
	};
	Type type;
	float value;
	vec4 color;
};

// A simple line plot of an array of values.
void plot(char const* label, initializer_list<PlotValues> values, initializer_list<PlotMarker> markers = {}, uint32 height = 0, bool stacked = false);

}
