module;
#include <utility>
#include <vector>
#include "vuk/vsl/Core.hpp"
#include "vuk/Types.hpp"

export module playnote.gfx.renderer;

import playnote.stx.math;
import playnote.sys.gpu;

namespace playnote::gfx {

using stx::ivec2;
using stx::vec4;
using sys::ManagedImage;

export class Renderer {
public:
	struct Rect {
		ivec2 pos;
		ivec2 size;
		vec4 color;
	};

	explicit Renderer(sys::GPU& gpu);

	// Add a solid color rectangle to the draw queue
	void enqueue_rect(Rect);

	// Draw all enqueued entities and clear queue
	void draw();

private:
	sys::GPU& gpu;
	std::vector<Rect> rects{};
};

Renderer::Renderer(sys::GPU& gpu):
	gpu{gpu}
{
	constexpr auto rects_vert_src = std::to_array<uint>({
#include "spv/rects.vert.spv"
	});
	constexpr auto rects_frag_src = std::to_array<uint>({
#include "spv/rects.frag.spv"
	});
	auto pci = vuk::PipelineBaseCreateInfo{};
	pci.add_static_spirv(rects_vert_src.data(), rects_vert_src.size(), "rects.vert");
	pci.add_static_spirv(rects_frag_src.data(), rects_frag_src.size(), "rects.frag");
	gpu.get_global_allocator().get_context().create_named_pipeline("rects", pci);
}

void Renderer::enqueue_rect(Rect rect)
{
	rects.emplace_back(rect);
}

void Renderer::draw()
{
	gpu.frame([this](auto& allocator, auto&& target) -> ManagedImage {
		auto [rects_buf, rects_fut] = vuk::create_buffer(allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(rects));
		auto cleared = vuk::clear_image(std::move(target), vuk::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});

		auto pass = vuk::make_pass("rects",
			[rects_len = rects.size(), rects_buf = *rects_buf](vuk::CommandBuffer& cmd, VUK_IA(vuk::eColorWrite) target) {
			cmd.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
			   .set_viewport(0, vuk::Rect2D::framebuffer())
			   .set_scissor(0, vuk::Rect2D::framebuffer())
			   .set_rasterization({})
			   .broadcast_color_blend({})
			   .bind_graphics_pipeline("rects")
			   .bind_buffer(0, 0, rects_buf)
			   .draw(6 * rects_len, 1, 0, 0);

			return target;
		});
		return pass(std::move(cleared));
	});

	rects.clear();
}

}
