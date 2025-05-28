/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/renderer.cppm:
A renderer of primitives.
*/

module;
#include "macros/vuk.hpp"

export module playnote.gfx.renderer;

import playnote.preamble;
import playnote.lib.vulkan;
import playnote.dev.gpu;
import playnote.gfx.imgui;

namespace playnote::gfx {

namespace vk = lib::vk;

export class Renderer {
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
		void enqueue_rect(Rect rect) { rects.emplace_back(rect); }

	private:
		friend Renderer;
		vector<Rect> rects{};
	};

	explicit Renderer(dev::GPU& gpu);

	// Provide a queue to the function argument, and then draw contents of the queue to the screen.
	// Each call will wait block until the next frame begins.
	// imgui:: calls are only allowed within the function argument.
	template<callable<void(Queue&)> Func>
	void frame(Func&&);

private:
	dev::GPU& gpu;
	gfx::Imgui imgui;
};

Renderer::Renderer(dev::GPU& gpu):
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
void Renderer::frame(Func&& func)
{
	auto queue = Queue{};
	imgui.enqueue([&]() { func(queue); });

	gpu.frame([this, &queue](auto& allocator, auto&& target) -> vk::ManagedImage {
		auto cleared = vk::clear_image(move(target), {0.0f, 0.0f, 0.0f, 1.0f});
		auto rects = vk::create_scratch_buffer(allocator, span(queue.rects));

		auto pass = vk::make_pass("rects",
			[window_size = gpu.get_window().size(), rects, rects_count = queue.rects.size()]
			(vk::CommandBuffer& cmd, VUK_IA(vk::Access::eColorWrite) target) {
			vk::set_cmd_defaults(cmd)
				.bind_graphics_pipeline("rects")
				.bind_buffer(0, 0, rects)
				.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
				.draw(6 * rects_count, 1, 0, 0);
			return target;
		});
		auto rects_drawn =  pass(move(cleared));
		return imgui.draw(allocator, move(rects_drawn));
	});
}

}
