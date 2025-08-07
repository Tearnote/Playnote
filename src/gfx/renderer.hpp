/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/renderer.hpp:
A renderer of primitives.
*/

#pragma once
#include "preamble.hpp"
#include "lib/vulkan.hpp"
#include "dev/gpu.hpp"
#include "gfx/imgui.hpp"

namespace playnote::gfx {

namespace vk = lib::vk;

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

	explicit Renderer(dev::GPU& gpu);

	// Provide a queue to the function argument, and then draw contents of the queue to the screen.
	// Each call will wait block until the next frame begins.
	// imgui:: calls are only allowed within the function argument.
	template<callable<void(Queue&)> Func>
	void frame(initializer_list<id> layer_order, Func&&);

private:
	dev::GPU& gpu;
	Imgui imgui;

	[[nodiscard]] auto draw_rects(vk::Allocator&, vk::ManagedImage&&, span<Rect const>) -> vk::ManagedImage;
};

inline Renderer::Renderer(dev::GPU& gpu):
	gpu{gpu},
	imgui{gpu}
{
	constexpr auto rects_vert_src = to_array<uint32>({
#include "spv/rects.vert.spv"
	});
	constexpr auto rects_frag_src = to_array<uint32>({
#include "spv/rects.frag.spv"
	});
	auto& context = gpu.get_global_allocator().get_context();
	vk::create_graphics_pipeline(context, "rects", rects_vert_src, rects_frag_src);
}

template<callable<void(Renderer::Queue&)> Func>
void Renderer::frame(initializer_list<id> layer_order, Func&& func)
{
	auto queue = Queue{};
	imgui.enqueue([&]() { func(queue); });

	gpu.frame([&, this](auto& allocator, auto&& target) -> vk::ManagedImage {
		auto next = vk::clear_image(move(target), {0.0f, 0.0f, 0.0f, 1.0f});

		for (auto const& layer: layer_order |
			views::filter([&](auto id) { return queue.layers.contains(id); }) |
			views::transform([&](auto id) -> Queue::Layer const& { return queue.layers.at(id); })) {
			auto result = draw_rects(allocator, move(next), layer.rects);
			next = move(result);
		}
		return imgui.draw(allocator, move(next));
	});
}

inline auto Renderer::draw_rects(vk::Allocator& allocator, vk::ManagedImage&& dest, span<Rect const> rects) -> vk::ManagedImage
{
	auto rects_buf = vk::create_scratch_buffer(allocator, span{rects});
	auto pass = vk::make_pass("rects",
		[window_size = gpu.get_window().size(), rects_buf, rects_count = rects.size()]
		(vk::CommandBuffer& cmd, VUK_IA(vk::Access::eColorWrite) target) {
		vk::set_cmd_defaults(cmd)
			.bind_graphics_pipeline("rects")
			.bind_buffer(0, 0, rects_buf)
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.draw(6 * rects_count, 1, 0, 0);
		return target;
	});
	return pass(move(dest));
}

}
