module;
#include <array>
#include "vuk/runtime/vk/Pipeline.hpp"
#include "vuk/vsl/Core.hpp"
#include "vuk/RenderGraph.hpp"

module playnote.gfx.renderer;

namespace playnote::gfx {

Renderer::Renderer(sys::GPU& gpu):
	gpu{gpu},
	imgui{gpu}
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

void Renderer::draw(Queue const& queue)
{
	gpu.frame([this, &queue](auto& allocator, auto&& target) -> ManagedImage {
		auto [rects_buf, rects_fut] = vuk::create_buffer(allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(queue.rects));
		auto cleared = vuk::clear_image(std::move(target), vuk::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});

		auto pass = vuk::make_pass("rects",
			[window_size = gpu.get_window().size(), rects_len = queue.rects.size(), rects_buf = *rects_buf]
			(vuk::CommandBuffer& cmd, VUK_IA(vuk::eColorWrite) target) {
			cmd.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
			   .set_viewport(0, vuk::Rect2D::framebuffer())
			   .set_scissor(0, vuk::Rect2D::framebuffer())
			   .set_rasterization({})
			   .broadcast_color_blend({})
			   .bind_graphics_pipeline("rects")
			   .bind_buffer(0, 0, rects_buf)
			   .specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			   .draw(6 * rects_len, 1, 0, 0);

			return target;
		});
		auto rects_drawn =  pass(std::move(cleared));
		return imgui.draw(allocator, std::move(rects_drawn));
	});
}

}
