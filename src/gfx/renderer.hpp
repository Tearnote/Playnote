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
	// Solid color rectangle primitive.
	struct Rect {
		ivec2 pos;
		ivec2 size;
		vec4 color;
	};

	// An accumulator of primitives to draw.
	class Queue {
	public:
		// Add a solid color rectangle to the draw queue.
		void enqueue_rect(id layer, Rect rect) { layers[layer].rects.emplace_back(rect); }

	private:
		struct Layer {
			vector<Rect> rects;
		};

		friend Renderer;
		unordered_map<id, Layer> layers;
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

	[[nodiscard]] auto draw_rects(lib::vuk::Allocator&, lib::vuk::ManagedImage&&, span<Rect const>) -> lib::vuk::ManagedImage;
};

inline Renderer::Renderer(dev::Window& window, Logger::Category cat):
	cat{cat},
	gpu{window, cat},
	imgui{gpu}
{
	constexpr auto rects_vert_src = to_array<uint32>({
#include "spv/rects.vert.spv"
	});
	constexpr auto rects_frag_src = to_array<uint32>({
#include "spv/rects.frag.spv"
	});
	auto& context = gpu.get_global_allocator().get_context();
	lib::vuk::create_graphics_pipeline(context, "rects", rects_vert_src, rects_frag_src);
	DEBUG_AS(cat, "Compiled rects pipeline");

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
			views::transform([&](auto id) -> Queue::Layer const& { return queue.layers.at(id); })) {
			auto result = draw_rects(allocator, move(next), layer.rects);
			next = move(result);
		}
		return imgui.draw(allocator, move(next));
	});
}

inline auto Renderer::draw_rects(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest, span<Rect const> rects) -> lib::vuk::ManagedImage
{
	auto rects_buf = lib::vuk::create_scratch_buffer(allocator, span{rects});
	auto pass = lib::vuk::make_pass("rects",
		[window_size = gpu.get_window().size(), window_scale = gpu.get_window().scale(), rects_buf, rects_count = rects.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eColorWrite) target) {
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

}
