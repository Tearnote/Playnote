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
#include "lib/vuk.hpp"
#include "dev/window.hpp"
#include "dev/gpu.hpp"
#include "gfx/imgui.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Types.hpp"

namespace playnote::gfx {

class Renderer {
public:
#include "gpu/shared/rects.slang.h"
#include "gpu/shared/primitives.slang.h"
#include "gpu/shared/tiles.slang.h"

	// An accumulator of primitives to draw.
	class Queue {
	public:
		// Add a solid color rectangle to the draw queue.
		auto enqueue_rect(id layer, Rect) -> Queue&;

		auto add_circle(Primitive) -> Queue&;

	private:
		struct Layer {
			vector<Rect> rects;
		};

		friend Renderer;
		unordered_map<id, Layer> layers;
		vector<Primitive> primitives;
	};

	explicit Renderer(dev::Window&, Logger::Category);

	// Provide a queue to the function argument, and then draw contents of the queue to the screen.
	// Each call will wait block until the next frame begins.
	// imgui:: calls are only allowed within the function argument.
	template<callable<void(Queue&)> Func>
	void frame(initializer_list<id> layer_order, Func&&);

private:
	Logger::Category cat;
	dev::GPU gpu;
	Imgui imgui;

	auto generate_tile_lists(lib::vuk::Allocator&, span<Primitive const>) -> tuple<lib::vuk::ManagedBuffer, lib::vuk::ManagedBuffer>;
	[[nodiscard]] auto draw_rects(lib::vuk::Allocator&, lib::vuk::ManagedImage&&,
		span<Rect const>) -> lib::vuk::ManagedImage;
	[[nodiscard]] auto draw_primitives(lib::vuk::Allocator&, lib::vuk::ManagedImage&&, span<Primitive const>) -> lib::vuk::ManagedImage;
};

inline auto srgb_decode(float4 color) -> float4
{
	auto const r = color.r() < 0.04045 ? (1.0 / 12.92) * color.r() : pow((color.r() + 0.055) * (1.0 / 1.055), 2.4);
	auto const g = color.g() < 0.04045 ? (1.0 / 12.92) * color.g() : pow((color.g() + 0.055) * (1.0 / 1.055), 2.4);
	auto const b = color.b() < 0.04045 ? (1.0 / 12.92) * color.b() : pow((color.b() + 0.055) * (1.0 / 1.055), 2.4);
	return {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), color.a()};
}

inline auto Renderer::Queue::enqueue_rect(id layer, Rect rect) -> Queue&
{
	rect.color = srgb_decode(rect.color);
	layers[layer].rects.emplace_back(rect);
	return *this;
}

inline auto Renderer::Queue::add_circle(Primitive primitive) -> Queue&
{
	primitive.color = srgb_decode(primitive.color);
	primitives.emplace_back(primitive);
	return *this;
}

inline Renderer::Renderer(dev::Window& window, Logger::Category cat):
	cat{cat},
	gpu{window, cat},
	imgui{gpu}
{
	auto& context = gpu.get_global_allocator().get_context();

#include "spv/rects.slang.spv.h"
	lib::vuk::create_graphics_pipeline(context, "rects", rects_spv);
	DEBUG_AS(cat, "Compiled rects pipeline");

#include "spv/gen_tiles.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "gen_tiles", gen_tiles_spv);
	DEBUG_AS(cat, "Compiled gen_tiles pipeline");

#include "spv/primitives.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "primitives", primitives_spv);
	DEBUG_AS(cat, "Compiled primitives pipeline");

	INFO_AS(cat, "Renderer initialized");
}

template<callable<void(Renderer::Queue&)> Func>
void Renderer::frame(initializer_list<id> layer_order, Func&& func)
{
	auto queue = Queue{};
	imgui.enqueue([&]() { func(queue); });

	gpu.frame([&, this](auto& allocator, auto&& target) -> lib::vuk::ManagedImage {
		auto next = lib::vuk::clear_image(move(target), {0.0f, 0.0f, 0.0f, 1.0f});

		for (auto const& layer: layer_order |
			views::filter([&](auto id) { return queue.layers.contains(id); }) |
			views::transform([&](auto id) -> Queue::Layer const& { return queue.layers.at(id); }))
		{
			auto result = draw_rects(allocator, move(next), layer.rects);
			next = move(result);
		}
		if (!queue.primitives.empty()) {
			auto [refs_buf, ref_count_buf] = generate_tile_lists(allocator, queue.primitives);
			next = draw_primitives(allocator, move(next), queue.primitives);
		}
		return imgui.draw(allocator, move(next));
	});
}

inline auto Renderer::generate_tile_lists(lib::vuk::Allocator& allocator,
	span<Primitive const> primitives) -> tuple<lib::vuk::ManagedBuffer, lib::vuk::ManagedBuffer>
{
	auto const window_size = gpu.get_window().size();
	auto const tile_bound = (window_size + int2{15, 15}) % int2{16, 16};
	auto ref_count_init = lib::vuk::DispatchIndirectCommand{0, 1, 1};
	auto primitives_buf = lib::vuk::create_scratch_buffer(allocator, primitives);
	auto refs_buf = lib::vuk::declare_buf("refs");
	refs_buf->size = sizeof(TileRef) * tile_bound.x() * tile_bound.y() * primitives.size();
	refs_buf->memory_usage = lib::vuk::MemoryUsage::eGPUonly;
	auto ref_count_buf_raw = lib::vuk::create_scratch_buffer(allocator, span{&ref_count_init, 1});
	auto ref_count_buf = lib::vuk::acquire_buf("ref_count", ref_count_buf_raw, lib::vuk::Access::eHostWrite);

	auto pass = lib::vuk::make_pass("gen_tiles",
		[window_size, primitives_buf, primitives_count = primitives.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_BA(lib::vuk::eComputeWrite) refs_buf, VUK_BA(lib::vuk::eComputeRW) ref_count_buf)
	{
		cmd
			.bind_compute_pipeline("gen_tiles")
			.bind_buffer(0, 0, primitives_buf)
			.bind_buffer(0, 1, refs_buf)
			.bind_buffer(0, 2, ref_count_buf)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, static_cast<int>(primitives_count))
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.dispatch_invocations(primitives_count, 1, 1);
		return make_tuple(refs_buf, ref_count_buf);
	});
	return pass(move(refs_buf), move(ref_count_buf));
}

inline auto Renderer::draw_rects(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest,
	span<Rect const> rects) -> lib::vuk::ManagedImage
{
	auto rects_buf = lib::vuk::create_scratch_buffer(allocator, rects);
	auto pass = lib::vuk::make_pass("rects",
		[window_size = gpu.get_window().size(), window_scale = gpu.get_window().scale(), rects_buf, rects_count = rects.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eColorWrite) target)
	{
		lib::vuk::set_cmd_defaults(cmd)
			.bind_graphics_pipeline("rects")
			.bind_buffer(0, 0, rects_buf)
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, window_scale)
			.draw(6 * rects_count, 1, 0, 0);
		return target;
	});
	return pass(move(dest));
}

inline auto Renderer::draw_primitives(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest, span<Primitive const> primitives) -> lib::vuk::ManagedImage
{
	auto primitives_buf = lib::vuk::create_scratch_buffer(allocator, primitives);
	auto pass = lib::vuk::make_pass("circles",
		[window_size = gpu.get_window().size(), window_scale = gpu.get_window().scale(), primitives_buf, primitives_count = primitives.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eComputeRW) target)
	{
		cmd
			.bind_compute_pipeline("primitives")
			.bind_buffer(0, 0, primitives_buf)
			.bind_image(0, 1, target)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, static_cast<int>(primitives_count))
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, window_scale)
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest));
}

}
