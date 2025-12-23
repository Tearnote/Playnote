/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "dev/window.hpp"
#include "dev/gpu.hpp"
#include "gfx/imgui.hpp"
#include "gfx/text.hpp"

namespace playnote::gfx {

// Renderer of all on-screen shapes and glyphs.
class Renderer {
public:
#include "gpu/shared/primitive.slang.h"

	static constexpr auto VirtualViewportSize = float2{900.0f, 480.0f};
	static constexpr auto VirtualViewportMargin = 40.0f; // in logical pixels

	// Properties shared by all shapes.
	struct Drawable {
		float2 position; // Center of the shape
		float2 velocity; // Delta from previous frame's position
		float4 color;    // [0.0-1.0] RGBA, in sRGB
		int depth;       // 0 or higher; smaller depth value = in front. If depth is equal to
		                 // another overlapping shape, the order is unspecified and might flicker.
	};

	struct RectParams {
		float2 size; // Total width and height
	};

	struct CircleParams {
		float radius;
	};

	struct TextParams {
		float size = 1.0f; // em-height
		float line_height = 1.0f; // in size-scaled ems
	};

	enum class TextStyle {
		SansMedium,
		SansBold,
	};

	// An accumulator of primitives to draw.
	class Queue {
	public:
		// Draw a group of several shapes as a compound shape by enqueuing them within
		// the provided function. Shapes in the same group must have the same depth.
		template<callable<void()> Func>
		void group(Func&&);

		// Convert a position from physical window coordinates to logical coordinates.
		auto physical_to_logical(float2) -> float2;

		// Convert a position from logical window coordinates to physical coordinates.
		auto logical_to_physical(float2) -> float2;

		// Enqueue shapes for drawing.

		auto rect(Drawable, RectParams) -> Queue&;
		auto rect_tl(Drawable, RectParams) -> Queue&; // Position in top-left rather than center
		auto circle(Drawable, CircleParams) -> Queue&;
		auto text(Text const&, Drawable, TextParams) -> Queue&;

	private:
		friend class Renderer;

		struct GlyphParams {
			AABB<float> atlas_bounds;
			float size;
			int page;
		};

		bool inside_group = false;
		vector<tuple<Drawable, RectParams, int>> rects; // third: group id
		vector<tuple<Drawable, CircleParams, int>> circles; // third: group id
		vector<tuple<Drawable, GlyphParams, int>> glyphs; // third: group id
		mutable vector<pair<int, int>> group_depths; // first: group id (initially equal to index), second: depth
		float4 transform;
		float4 inv_transform;

		Queue(float4 transform, float4 inv_transform): transform{transform}, inv_transform{inv_transform} {}
		[[nodiscard]] auto to_primitive_list() const -> vector<Primitive>;
	};

	// Create a renderer for the given window. Manages the window's GPU context.
	Renderer(dev::Window&, Logger::Category);

	// Create a text renderable from a string. Store and reuse.
	auto prepare_text(TextStyle, string_view, optional<float> max_width = nullopt) -> Text;

	// Provide a queue to the function argument, and then draw contents of the queue to the screen.
	// Each call will wait block until the next frame begins.
	// imgui:: calls are only allowed within the function argument.
	template<callable<void(Queue&)> Func>
	void frame(Func&&);

private:
	Logger::Category cat;
	dev::GPU gpu;
	Imgui imgui;
	TextShaper text_shaper;
	lib::vuk::Texture static_atlas;
	lib::vuk::Texture dynamic_atlas;

	auto create_queue() -> Queue;
	void draw_frame(Queue&&);
};

template<callable<void()> Func>
inline void Renderer::Queue::group(Func&& func)
{
	inside_group = true;
	group_depths.emplace_back(group_depths.size(), -1);
	func();
	inside_group = false;
	if (group_depths.back().second == -1) group_depths.pop_back();
}

template<callable<void(Renderer::Queue&)> Func>
void Renderer::frame(Func&& func)
{
	auto queue = create_queue();
	imgui.enqueue([&]() { func(queue); });
	draw_frame(move(queue));
}

}
