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

namespace playnote::gfx {

class Renderer {
public:
#include "gpu/shared/rects.slang.h"
#include "gpu/shared/primitives.slang.h"

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

	[[nodiscard]] auto draw_rects(lib::vuk::Allocator&, lib::vuk::ManagedImage&&,
		span<Rect const>) -> lib::vuk::ManagedImage;
	[[nodiscard]] auto draw_primitives(lib::vuk::Allocator&, lib::vuk::ManagedImage&&,
		span<Primitive const>) -> lib::vuk::ManagedImage;
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
		if (!queue.primitives.empty())
			next = draw_primitives(allocator, move(next), queue.primitives);
		return imgui.draw(allocator, move(next));
	});
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

inline auto Renderer::draw_primitives(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest,
		span<Primitive const> primitives) -> lib::vuk::ManagedImage
{
	auto primitives_buf = lib::vuk::create_scratch_buffer(allocator, primitives);
	auto pass = lib::vuk::make_pass("circles",
		[window_size = gpu.get_window().size(), window_scale = gpu.get_window().scale(), primitives_buf, primitives_count = primitives.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eComputeRW) target)
	{
		lib::vuk::set_cmd_defaults(cmd)
			.bind_compute_pipeline("primitives")
			.bind_buffer(0, 0, primitives_buf)
			.bind_image(0, 1, target)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, primitives_count)
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, window_scale)
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest));
}

}
