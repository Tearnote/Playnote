/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/imgui.cppm:
Imgui initialization and usage.
Parts are adapted from vuk's built-in Imgui integration; reproduced here under the MIT license.
*/

module;
#include "backends/imgui_impl_glfw.h"
#include "implot.h"
#include "quill/bundled/fmt/base.h"
#include "imgui.h"
#include "vuk/runtime/vk/AllocatorHelpers.hpp"
#include "vuk/runtime/vk/PipelineTypes.hpp"
#include "vuk/runtime/vk/VkRuntime.hpp"
#include "vuk/runtime/vk/Pipeline.hpp"
#include "vuk/runtime/vk/Image.hpp"
#include "vuk/vsl/Core.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Buffer.hpp"
#include "vuk/Types.hpp"
#include "vuk/Value.hpp"
#include "macros/assert.hpp"

export module playnote.lib.imgui;

import playnote.preamble;
import playnote.lib.vulkan;
import playnote.lib.glfw;

namespace playnote::lib::imgui {

// Bundle of resources used in Imgui integration.
export struct Context {
	vuk::Unique<vuk::Image> font_image;
	vuk::Unique<vuk::ImageView> font_image_view;
	vuk::SamplerCreateInfo font_sci;
	vuk::ImageAttachment font_ia;
	vector<vuk::Value<vuk::SampledImage>> sampled_images;
};

// Initialize Imgui and relevant GPU resources.
// Throws if vuk throws.
export auto init(glfw::Window window, vk::Allocator& global_allocator) -> Context
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window, true);

	// Function body below adapted from vuk::extra::ImGui_ImplVuk_Init()

	auto& ctx = global_allocator.get_context();
	auto& io = ImGui::GetIO();
	io.BackendRendererName = "imgui_impl_vuk";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

	auto config = ImFontConfig{};
	config.PixelSnapH = true;
	config.OversampleH = 1;
	config.OversampleV = 1;
	auto ranges = ImVector<ImWchar>{};
	auto builder = ImFontGlyphRangesBuilder{};
	builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
	builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
	builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
	builder.BuildRanges(&ranges);
	ASSERT(io.Fonts->AddFontFromFileTTF("assets/unifont-16.0.03.ttf", 16.0f, &config, ranges.Data));
	auto* pixels = static_cast<unsigned char*>(nullptr);
	auto width = 0;
	auto height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	auto font_ia = vuk::ImageAttachment::from_preset(
		vuk::ImageAttachment::Preset::eMap2D, vuk::Format::eR8G8B8A8Srgb,
		vuk::Extent3D{static_cast<unsigned>(width), static_cast<unsigned>(height), 1u},
		vuk::Samples::e1);
	font_ia.level_count = 1;
	auto [image, view, fut] = vuk::create_image_and_view_with_data(global_allocator, vuk::DomainFlagBits::eTransferOnTransfer, font_ia, pixels);
	auto comp = vuk::Compiler{};
	fut.as_released(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
	constexpr auto sci = vuk::SamplerCreateInfo{
		.magFilter = vuk::Filter::eLinear,
		.minFilter = vuk::Filter::eLinear,
		.mipmapMode = vuk::SamplerMipmapMode::eLinear,
		.addressModeU = vuk::SamplerAddressMode::eRepeat,
		.addressModeV = vuk::SamplerAddressMode::eRepeat,
		.addressModeW = vuk::SamplerAddressMode::eRepeat,
	};
	auto imgui_ctx = Context{
		.font_image = move(image),
		.font_image_view = move(view),
		.font_sci = sci,
		.font_ia = *fut.get(global_allocator, comp),
	};
	imgui_ctx.font_ia.layout = vuk::ImageLayout::eReadOnlyOptimal;
	ctx.set_name(imgui_ctx.font_image_view->payload, "ImGui/font");

	constexpr auto imgui_vert_src = to_array<uint32>({
#include "spv/imgui.vert.spv"
	});
	constexpr auto imgui_frag_src = to_array<uint32>({
#include "spv/imgui.frag.spv"
	});
	vk::create_graphics_pipeline(ctx, "imgui", imgui_vert_src, imgui_frag_src);

	return imgui_ctx;
}

// Mark the start of a new frame for Imgui. All Imgui commands must come after this is called.
export void begin()
{
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

// Finalize Imgui commands and prepare buffers for rendering. Imgui commands cannot be used anymore.
export void end() { ImGui::Render(); }

// Draw queued Imgui commands onto the provided image.
// Throws if vuk throws.
export auto render(vk::Allocator& frame_allocator, vk::ManagedImage&& target, Context& context) -> vk::ManagedImage
{
	// Function body below adapted from vuk::extra::ImGui_ImplVuk_Render()

	auto draw_data = ImGui::GetDrawData();
	auto reset_render_state = [](Context const& data, vuk::CommandBuffer& cmd, ImDrawData const* draw_data, vuk::Buffer const& vertex, vuk::Buffer const& index) {
		cmd.bind_image(0, 0, *data.font_image_view).bind_sampler(0, 0, data.font_sci)
		 .bind_vertex_buffer(0, vertex, 0, vuk::Packed{vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm})
		 .bind_graphics_pipeline("imgui")
		 .set_viewport(0, vuk::Rect2D::framebuffer());
		if (index.size > 0)
			cmd.bind_index_buffer(index, sizeof(ImDrawIdx) == 2? vuk::IndexType::eUint16 : vuk::IndexType::eUint32);
		struct PC {
			array<float, 2> scale;
			array<float, 2> translate;
		};
		auto pc = PC{};
		pc.scale[0] = 2.0f / draw_data->DisplaySize.x;
		pc.scale[1] = 2.0f / draw_data->DisplaySize.y;
		pc.translate[0] = -1.0f - draw_data->DisplayPos.x * pc.scale[0];
		pc.translate[1] = -1.0f - draw_data->DisplayPos.y * pc.scale[1];
		cmd.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);
	};

	auto vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
	auto index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
	auto imvert = *vuk::allocate_buffer(frame_allocator, {vuk::MemoryUsage::eCPUtoGPU, vertex_size, 1 });
	auto imind = *vuk::allocate_buffer(frame_allocator, {vuk::MemoryUsage::eCPUtoGPU, index_size, 1 });

	auto vtx_dst = 0zu;
	auto idx_dst = 0zu;
	for (auto const n: views::iota(0, draw_data->CmdListsCount)) {
		auto const* cmd_list = draw_data->CmdLists[n];
		auto imverto = imvert->add_offset(vtx_dst * sizeof(ImDrawVert));
		auto imindo = imind->add_offset(idx_dst * sizeof(ImDrawIdx));

		copy(span{cmd_list->VtxBuffer.Data, static_cast<long unsigned int>(cmd_list->VtxBuffer.Size)}, reinterpret_cast<ImDrawVert*>(imverto.mapped_ptr));
		copy(span{cmd_list->IdxBuffer.Data, static_cast<long unsigned int>(cmd_list->IdxBuffer.Size)}, reinterpret_cast<ImDrawIdx*>(imindo.mapped_ptr));

		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}

	// add rendergraph dependencies to be transitioned
	ImGui::GetIO().Fonts->TexID = static_cast<ImTextureID>(context.sampled_images.size() + 1);
	context.sampled_images.emplace_back(vuk::combine_image_sampler("imgui font",
		vuk::acquire_ia("imgui font", context.font_ia, vuk::Access::eFragmentSampled),
		vuk::acquire_sampler("font sampler", context.font_sci)));
	// make all rendergraph sampled images available
	auto sampled_images_array = vuk::declare_array("imgui_sampled", span{context.sampled_images});

	auto pass = vuk::make_pass("imgui", [&context, verts = imvert.get(), inds = imind.get(), draw_data, reset_render_state](
		vuk::CommandBuffer& cmd, VUK_IA(vuk::Access::eColorWrite) dst, VUK_ARG(vuk::SampledImage[], vuk::Access::eFragmentSampled) sis) {
		cmd.set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
		   .set_rasterization({})
		   .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend);
		reset_render_state(context, cmd, draw_data, verts, inds);
		// Will project scissor/clipping rectangles into framebuffer space
		auto const clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
		auto const clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// Render command lists
		// (Because we merged all buffers into a single one, we maintain our own offset into them)
		auto global_vtx_offset = 0;
		auto global_idx_offset = 0;
		for (auto const n: views::iota(0, draw_data->CmdListsCount)) {
			auto const* cmd_list = draw_data->CmdLists[n];
			for (auto const cmd_i: views::iota(0, cmd_list->CmdBuffer.Size)) {
				auto const* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback != nullptr) {
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
						reset_render_state(context, cmd, draw_data, verts, inds);
					else
						pcmd->UserCallback(cmd_list, pcmd);
				} else {
					// Project scissor/clipping rectangles into framebuffer space
					auto clip_rect = ImVec4(
						(pcmd->ClipRect.x - clip_off.x) * clip_scale.x,
						(pcmd->ClipRect.y - clip_off.y) * clip_scale.y,
						(pcmd->ClipRect.z - clip_off.x) * clip_scale.x,
						(pcmd->ClipRect.w - clip_off.y) * clip_scale.y
					);

					auto fb_width = cmd.get_ongoing_render_pass().extent.width;
					auto fb_height = cmd.get_ongoing_render_pass().extent.height;
					if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
						// Negative offsets are illegal for vkCmdSetScissor
						clip_rect.x = max(clip_rect.x, 0.0f);
						clip_rect.y = max(clip_rect.y, 0.0f);

						// Apply scissor/clipping rectangle
						auto scissor = vuk::Rect2D{
							.offset = {static_cast<int32>(clip_rect.x), static_cast<int32>(clip_rect.y)},
							.extent = {static_cast<uint32>(clip_rect.z - clip_rect.x), static_cast<uint32>(clip_rect.w - clip_rect.y)},
						};
						cmd.set_scissor(0, scissor);

						if (pcmd->GetTexID()) {
							auto const ia_index = static_cast<size_t>(pcmd->GetTexID()) - 1;
							cmd.bind_image(0, 0, sis[ia_index].ia)
							   .bind_sampler(0, 0, sis[ia_index].sci);
						}
						cmd.draw_indexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
					}
				}
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}

		context.sampled_images.clear();
		return dst;
	});

	return pass(target, move(sampled_images_array));
}

// Start a new ImGui window. Initial position and size will be chosen automatically.
export void begin_window(char const* title)
{
	ImGui::Begin(title);
}

// Start a new ImGui window at a specific position and size. If static_frame is true, the window
// will have no title and won't be able to be modified by the user.
export void begin_window(char const* title, uvec2 pos, uint width, bool static_frame = false)
{
	ImGui::SetNextWindowPos({static_cast<float>(pos.x()), static_cast<float>(pos.y())});
	ImGui::SetNextWindowSize({static_cast<float>(width), 0});
	ImGui::Begin(title, nullptr, static_frame? ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize : 0);
}

// Finalize a started window.
export void end_window()
{
	ImGui::End();
}

// Use before an element to keep it on the same line as the previous one.
export void same_line() { ImGui::SameLine(); }

// A clickable button. Returns true when clicked.
export auto button(char const* str) -> bool
{
	return ImGui::Button(str);
}

// Static text.
export void text(string_view str) { ImGui::TextWrapped("%s", string{str}.c_str()); }

// Static text, fmt overload.
export template<typename... Args>
void text(fmtquill::format_string<Args...> fmt, Args&&... args)
{
	ImGui::TextWrapped("%s", format(fmt, forward<Args>(args)...).c_str());
}

// A control for a float variable, with +/- buttons and direct value input via keyboard.
export void input_float(char const* str, float& value,
	float step = 0.0f, float step_fast = 0.0f, char const* format = "%.3f")
{
	ImGui::InputFloat(str, &value, step, step_fast, format);
}

// A non-interactive progress bar control. If progress is nullopt, the bar will look intederminate.
export void progress_bar(optional<float> progress, string_view text)
{
	if (progress)
		ImGui::ProgressBar(*progress, ImVec2{-1.0f, 0.0f}, string{text}.c_str());
	else
		ImGui::ProgressBar(-1.0f * (static_cast<float>(ImGui::GetTime()) / 2.0f), ImVec2{-1.0f, 0.0f}, string{text}.c_str());
}

export struct PlotValues {
	char const* name;
	span<float const> data;
	vec4 color;
};

export struct PlotMarker {
	enum Type {
		Horizontal,
		Vertical,
	};
	Type type;
	float value;
	vec4 color;
};

// A simple line plot of an array of values.
export void plot(char const* label, initializer_list<PlotValues> values, initializer_list<PlotMarker> markers = {}, uint32 height = 0, bool stacked = false)
{
	if (!ImPlot::BeginPlot(label, ImVec2{-1, static_cast<float>(height)}, ImPlotFlags_NoLegend | ImPlotFlags_NoInputs | ImPlotFlags_NoFrame)) return;
	ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);

	struct ValueRef {
		span<PlotValues const> values;
		usize idx;
	};

	auto const value_func = [&]() -> ImPlotGetter {
		if (!stacked) {
			return [](int idx, void* userdata) -> ImPlotPoint {
				auto const& value_ref = *static_cast<ValueRef*>(userdata);
				return {
					static_cast<double>(idx),
					value_ref.values[value_ref.idx].data[idx]
				};
			};
		}
		return [](int idx, void* userdata) -> ImPlotPoint {
			auto const& value_ref = *static_cast<ValueRef*>(userdata);
			return {
				static_cast<double>(idx),
				fold_left(value_ref.values | views::take(value_ref.idx + 1), 0.0f,
					[&](float sum, auto const& value) { return sum + value.data[idx]; })
			};
		};
	}();

	for (auto [idx, value]: views::zip(views::iota(0zu), values) | views::reverse) {
		auto const implot_color = ImVec4{value.color.r(), value.color.g(), value.color.b(), value.color.a()};
		ImPlot::SetNextLineStyle(implot_color);
		ImPlot::SetNextFillStyle(implot_color, 0.5f);
		auto ref = ValueRef{values, idx};
		ImPlot::PlotLineG(value.name, value_func, &ref, value.data.size(), ImPlotLineFlags_Shaded);
	}
	for (auto const& marker: markers) {
		ImPlot::SetNextLineStyle({marker.color.r(), marker.color.g(), marker.color.b(), marker.color.a()});
		ImPlot::PlotInfLines("Marker", &marker.value, 1, marker.type == PlotMarker::Type::Horizontal? ImPlotInfLinesFlags_Horizontal : 0);
	}

	ImPlot::EndPlot();
}

}
