/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "preamble/algorithm.hpp"
#include "utils/logger.hpp"
#include "lib/vuk.hpp"
#include "dev/window.hpp"
#include "dev/gpu.hpp"
#include "gfx/imgui.hpp"

namespace playnote::gfx {

class Renderer {
#include "gpu/shared/primitive.slang.h"
#include "gpu/shared/worklist.slang.h"

public:
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

	// An accumulator of primitives to draw.
	class Queue {
	public:
		// Draw a group of several shapes as a compound shape by enqueuing them within
		// the provided function. Shapes in the same group must have the same depth.
		template<callable<void()> Func>
		void group(Func&&);

		// Enqueue shapes for drawing.

		auto rect(Drawable, RectParams) -> Queue&;
		auto circle(Drawable, CircleParams) -> Queue&;

		// Internal.
		[[nodiscard]] auto to_primitive_list() const -> vector<Primitive>;

	private:
		bool inside_group = false;
		vector<tuple<Drawable, RectParams, int>> rects; // third: group id
		vector<tuple<Drawable, CircleParams, int>> circles; // third: group id
		mutable vector<pair<int, int>> group_depths; // first: group id (initially equal to index), second: depth

		void transform(Drawable&);
	};

	explicit Renderer(dev::Window&, Logger::Category);

	// Provide a queue to the function argument, and then draw contents of the queue to the screen.
	// Each call will wait block until the next frame begins.
	// imgui:: calls are only allowed within the function argument.
	template<callable<void(Queue&)> Func>
	void frame(Func&&);

private:
	Logger::Category cat;
	dev::GPU gpu;
	Imgui imgui;

	auto generate_worklists(lib::vuk::Allocator&, lib::vuk::Buffer const&) -> tuple<lib::vuk::ManagedBuffer, lib::vuk::ManagedBuffer>;
	[[nodiscard]] auto draw_all(lib::vuk::Allocator&, lib::vuk::ManagedImage&&, lib::vuk::Buffer const&) -> lib::vuk::ManagedImage;
};

inline auto srgb_decode(float4 color) -> float4
{
	auto const r = color.r() < 0.04045 ? (1.0 / 12.92) * color.r() : pow((color.r() + 0.055) * (1.0 / 1.055), 2.4);
	auto const g = color.g() < 0.04045 ? (1.0 / 12.92) * color.g() : pow((color.g() + 0.055) * (1.0 / 1.055), 2.4);
	auto const b = color.b() < 0.04045 ? (1.0 / 12.92) * color.b() : pow((color.b() + 0.055) * (1.0 / 1.055), 2.4);
	return {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), color.a()};
}

template<callable<void()> Func>
inline void Renderer::Queue::group(Func&& func)
{
	inside_group = true;
	group_depths.emplace_back(group_depths.size(), -1);
	func();
	inside_group = false;
	if (group_depths.back().second == -1) group_depths.pop_back();
}

inline auto Renderer::Queue::rect(Drawable common, RectParams rect) -> Queue&
{
	transform(common);
	if (!inside_group) group_depths.emplace_back(group_depths.size(), -1);
	rects.emplace_back(common, rect, group_depths.size() - 1);
	group_depths.back().second = common.depth;
	return *this;
}

inline auto Renderer::Queue::circle(Drawable common, CircleParams circle) -> Queue&
{
	transform(common);
	if (!inside_group) group_depths.emplace_back(group_depths.size(), -1);
	circles.emplace_back(common, circle, group_depths.size() - 1);
	group_depths.back().second = common.depth;
	return *this;
}

inline void Renderer::Queue::transform(Drawable& drawable)
{
	drawable.color = srgb_decode(drawable.color);
}

inline auto Renderer::Queue::to_primitive_list() const -> vector<Primitive>
{
	// Create a group remapping table so that the groups are sorted by depth
	sort(group_depths, [](auto const& a, auto const& b) {
		return a.second > b.second; // From largest to smallest depth value
	});
	auto group_remapping = vector<int>{};
	group_remapping.resize(group_depths.size());
	for (auto [idx, val]: group_depths | views::enumerate)
		group_remapping[val.first] = idx;

	auto primitives = vector<Primitive>{};
	primitives.reserve(rects.size() + circles.size());
	for (auto [common, rect, group]: rects) {
		primitives.emplace_back(Primitive{
			.type = Primitive::Type::Rect,
			.group_id = group_remapping[group],
			.position = common.position,
			.velocity = common.velocity,
			.color = common.color,
			.rect_params = Primitive::RectParams{.size = rect.size},
		});
	}
	for (auto [common, circle, group]: circles) {
		primitives.emplace_back(Primitive{
			.type = Primitive::Type::Circle,
			.group_id = group_remapping[group],
			.position = common.position,
			.velocity = common.velocity,
			.color = common.color,
			.circle_params = Primitive::CircleParams{.radius = circle.radius},
		});
	}
	return primitives;
}

inline Renderer::Renderer(dev::Window& window, Logger::Category cat):
	cat{cat},
	gpu{window, cat},
	imgui{gpu}
{
	auto& context = gpu.get_global_allocator().get_context();

#include "spv/worklist_gen.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "worklist_gen", worklist_gen_spv);
	DEBUG_AS(cat, "Compiled worklist_gen pipeline");

#include "spv/draw_all.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "draw_all", draw_all_spv);
	DEBUG_AS(cat, "Compiled draw_all pipeline");

	INFO_AS(cat, "Renderer initialized");
}

template<callable<void(Renderer::Queue&)> Func>
void Renderer::frame(Func&& func)
{
	auto queue = Queue{};
	imgui.enqueue([&]() { func(queue); });
	auto primitives = queue.to_primitive_list();

	gpu.frame([&, this](auto& allocator, auto&& target) -> lib::vuk::ManagedImage {
		auto next = lib::vuk::clear_image(move(target), {0.0f, 0.0f, 0.0f, 1.0f});
		if (!primitives.empty()) {
			auto primitives_buf = lib::vuk::create_scratch_buffer(allocator, span{primitives});
			auto [worklists_buf, worklist_sizes_buf] = generate_worklists(allocator, primitives_buf);
			next = draw_all(allocator, move(next), primitives_buf);
		}
		return imgui.draw(allocator, move(next));
	});
}

inline auto Renderer::generate_worklists(lib::vuk::Allocator& allocator,
	lib::vuk::Buffer const& primitives_buf) -> tuple<lib::vuk::ManagedBuffer, lib::vuk::ManagedBuffer>
{
	auto const window_size = gpu.get_window().size();
	auto const tile_bound = (window_size + int2{TILE_SIZE - 1, TILE_SIZE - 1}) % int2{TILE_SIZE, TILE_SIZE};
	auto const tile_count = tile_bound.x() * tile_bound.y();

	auto worklists_buf = lib::vuk::declare_buf("worklists");
	worklists_buf->size = tile_count * sizeof(WorklistItem) * WORKLIST_MAX_SIZE;
	worklists_buf->memory_usage = lib::vuk::MemoryUsage::eGPUonly;

	auto worklist_sizes = vector<int>{};
	worklist_sizes.resize(tile_count);
	auto worklist_sizes_buf_raw = lib::vuk::create_scratch_buffer(allocator, span{worklist_sizes});
	auto worklist_sizes_buf = lib::vuk::acquire_buf("worklist_sizes", worklist_sizes_buf_raw, lib::vuk::Access::eHostWrite);

	auto pass = lib::vuk::make_pass("worklist_gen",
		[window_size, primitives_buf, primitives_count = primitives_buf.size / sizeof(Primitive)]
		(lib::vuk::CommandBuffer& cmd, VUK_BA(lib::vuk::eComputeWrite) worklists_buf, VUK_BA(lib::vuk::eComputeRW) worklist_sizes_buf)
	{
		cmd
			.bind_compute_pipeline("worklist_gen")
			.bind_buffer(0, 0, primitives_buf)
			.bind_buffer(0, 1, worklists_buf)
			.bind_buffer(0, 2, worklist_sizes_buf)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, static_cast<int>(primitives_count))
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.dispatch_invocations(primitives_count, 1, 1);
		return make_tuple(worklists_buf, worklist_sizes_buf);
	});
	return pass(move(worklists_buf), move(worklist_sizes_buf));
}

inline auto Renderer::draw_all(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest, lib::vuk::Buffer const& primitives_buf) -> lib::vuk::ManagedImage
{
	auto pass = lib::vuk::make_pass("draw_all",
		[window_size = gpu.get_window().size(), primitives_buf, primitives_count = primitives_buf.size / sizeof(Primitive)]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eComputeRW) target)
	{
		cmd
			.bind_compute_pipeline("draw_all")
			.bind_buffer(0, 0, primitives_buf)
			.bind_image(0, 1, target)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, static_cast<int>(primitives_count))
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest));
}

}
