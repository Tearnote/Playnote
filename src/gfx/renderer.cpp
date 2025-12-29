/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "gfx/renderer.hpp"

#include "gfx/text.hpp"
#include "preamble.hpp"
#include "utils/config.hpp"
#include "utils/assets.hpp"
#include "lib/vuk.hpp"
#include "gpu/shaders.hpp"
#include "vuk/Types.hpp"

namespace playnote::gfx {

#include "gpu/shared/worklist.slang.h"
#include "gpu/shared/config.slang.h"

static constexpr auto LinearSampler = lib::vuk::SamplerCreateInfo{
	.magFilter = lib::vuk::Filter::eLinear,
	.minFilter = lib::vuk::Filter::eLinear,
};

auto srgb_decode(float4 color) -> float4
{
	auto const r = color.r() < 0.04045 ? (1.0 / 12.92) * color.r() : pow((color.r() + 0.055) * (1.0 / 1.055), 2.4);
	auto const g = color.g() < 0.04045 ? (1.0 / 12.92) * color.g() : pow((color.g() + 0.055) * (1.0 / 1.055), 2.4);
	auto const b = color.b() < 0.04045 ? (1.0 / 12.92) * color.b() : pow((color.b() + 0.055) * (1.0 / 1.055), 2.4);
	return {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), color.a()};
}

auto generate_transform(int2 window_size, float window_scale) -> float4
{
	auto const base_margin_physical = Renderer::VirtualViewportMargin * window_scale;
	auto const playable_area = float2{window_size} - float2{base_margin_physical * 2.0f, base_margin_physical * 2.0f};
	auto const scale_wh = playable_area / Renderer::VirtualViewportSize;
	auto const scale = min(scale_wh.x(), scale_wh.y());
	auto const virtual_viewport_size_physical = Renderer::VirtualViewportSize * float2{scale, scale};
	auto const margin = (float2{window_size} - virtual_viewport_size_physical) / float2{2.0f, 2.0f};
	return {margin.x(), margin.y(), scale, scale};
}

auto generate_worklists(dev::GPU& gpu, lib::vuk::Allocator& allocator, span<Renderer::Primitive const> primitives,
	float4 transform) -> tuple<lib::vuk::ManagedBuffer, lib::vuk::ManagedBuffer, lib::vuk::ManagedBuffer>
{
	auto const window_size = gpu.get_window().size();
	auto const tile_bound = (window_size + int2{TILE_SIZE - 1, TILE_SIZE - 1}) / int2{TILE_SIZE, TILE_SIZE};
	auto const tile_count = tile_bound.x() * tile_bound.y();

	auto primitives_buf = lib::vuk::create_gpu_buffer(allocator, span{primitives});

	auto worklists_buf = lib::vuk::declare_buf("worklists");
	worklists_buf->size = tile_count * sizeof(WorklistItem) * WORKLIST_MAX_SIZE;
	worklists_buf->memory_usage = lib::vuk::MemoryUsage::eGPUonly;

	auto worklist_sizes = vector<int>{};
	worklist_sizes.resize(tile_count);
	auto worklist_sizes_buf_raw = lib::vuk::create_scratch_buffer(allocator, span{worklist_sizes});
	auto worklist_sizes_buf = lib::vuk::acquire_buf("worklist_sizes", worklist_sizes_buf_raw, lib::vuk::Access::eNone);

	auto gen_pass = lib::vuk::make_pass("worklist_gen",
		[window_size, primitives_count = primitives.size(), transform] (
			lib::vuk::CommandBuffer& cmd,
			VUK_BA(lib::vuk::eComputeRW) primitives_buf,
			VUK_BA(lib::vuk::eComputeWrite) worklists_buf,
			VUK_BA(lib::vuk::eComputeRW) worklist_sizes_buf
		)
	{
		cmd
			.bind_compute_pipeline("worklist_gen")
			.bind_buffer(0, 0, primitives_buf)
			.bind_buffer(0, 1, worklists_buf)
			.bind_buffer(0, 2, worklist_sizes_buf)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, transform)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, sizeof(float4), static_cast<int>(primitives_count))
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, TextShaper::PixelsPerEm)
			.dispatch_invocations(primitives_count, 1, 1);
		return make_tuple(primitives_buf, worklists_buf, worklist_sizes_buf);
	});
	auto sort_pass = lib::vuk::make_pass("worklist_sort",
		[tile_count] (
			lib::vuk::CommandBuffer& cmd,
			VUK_BA(lib::vuk::eComputeRW) worklists_buf,
			VUK_BA(lib::vuk::eComputeRead) worklist_sizes_buf
		)
	{
		cmd
			.bind_compute_pipeline("worklist_sort")
			.bind_buffer(0, 0, worklists_buf)
			.bind_buffer(0, 1, worklist_sizes_buf)
			.dispatch_invocations(WORKLIST_MAX_SIZE / 2, tile_count, 1);
		return make_tuple(worklists_buf, worklist_sizes_buf);
	});

	auto [primitives_buf_processed, worklists_buf_generated, worklist_sizes_buf_generated] =
		gen_pass(move(primitives_buf), move(worklists_buf), move(worklist_sizes_buf));
	auto [worklists_buf_sorted, worklist_sizes_buf_sorted] =
		sort_pass(move(worklists_buf_generated), move(worklist_sizes_buf_generated));
	return make_tuple(primitives_buf_processed, worklists_buf_sorted, worklist_sizes_buf_sorted);
}

auto draw_all(dev::GPU& gpu, lib::vuk::ManagedImage&& dest,
	lib::vuk::ManagedBuffer&& primitives_buf, lib::vuk::ManagedBuffer&& worklists_buf,
	lib::vuk::ManagedBuffer&& worklist_sizes_buf, lib::vuk::ImageView static_atlas_iv,
	lib::vuk::ManagedImage&& dynamic_atlas_ia) -> lib::vuk::ManagedImage
{
	auto pass = lib::vuk::make_pass("draw_all",
		[window_size = gpu.get_window().size(), static_atlas_iv] (
			lib::vuk::CommandBuffer& cmd,
			VUK_IA(lib::vuk::Access::eComputeRW) target,
			VUK_BA(lib::vuk::Access::eComputeRead) primitives_buf,
			VUK_BA(lib::vuk::Access::eComputeRead) worklists_buf,
			VUK_BA(lib::vuk::Access::eComputeRead) worklist_sizes_buf,
			VUK_IA(lib::vuk::Access::eComputeSampled) dynamic_atlas_ia
		)
	{
		auto subpixel_rendering = SubpixelRenderingMode::None;
		if (globals::config->get_entry<bool>("graphics", "subpixel_rendering")) {
			auto const layout = globals::config->get_entry<string>("graphics", "subpixel_layout");
			if (layout == "RGB") subpixel_rendering = SubpixelRenderingMode::RGB;
		}
		cmd
			.bind_compute_pipeline("draw_all")
			.bind_buffer(0, 0, primitives_buf)
			.bind_buffer(0, 1, worklists_buf)
			.bind_buffer(0, 2, worklist_sizes_buf)
			.bind_image(0, 3, static_atlas_iv).bind_sampler(0, 3, LinearSampler)
			.bind_image(0, 4, dynamic_atlas_ia).bind_sampler(0, 4, LinearSampler)
			.bind_image(0, 5, target)
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, +subpixel_rendering)
			.specialize_constants(3, TextShaper::DistanceRange)
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest), move(primitives_buf), move(worklists_buf), move(worklist_sizes_buf), move(dynamic_atlas_ia));
}

auto Renderer::Queue::physical_to_logical(float2 pos) -> float2
{
	return (pos + float2{inv_transform.x(), inv_transform.y()}) * float2{inv_transform.z(), inv_transform.w()};
}

auto Renderer::Queue::logical_to_physical(float2 pos) -> float2
{
	return (pos + float2{transform.x(), transform.y()}) * float2{transform.z(), transform.w()};
}

auto Renderer::Queue::rect(Drawable common, RectParams params) -> Queue&
{
	if (!inside_group) group_depths.emplace_back(group_depths.size(), -1);
	rects.emplace_back(common, params, group_depths.size() - 1);
	group_depths.back().second = common.depth;
	return *this;
}

auto Renderer::Queue::rect_tl(Drawable common, RectParams params) -> Queue&
{
	common.position += params.size / float2{2.0, 2.0};
	return this->rect(common, params);
}

auto Renderer::Queue::circle(Drawable common, CircleParams params) -> Queue&
{
	if (!inside_group) group_depths.emplace_back(group_depths.size(), -1);
	circles.emplace_back(common, params, group_depths.size() - 1);
	group_depths.back().second = common.depth;
	return *this;
}

auto Renderer::Queue::text(Text const& text, Drawable common, TextParams params) -> Queue&
{
	for (auto [line_idx, line]: text.lines | views::enumerate) {
		auto const line_offset = float2{0.0f, params.line_height * params.size * line_idx};
		for (auto const& glyph: line.glyphs) {
			if (!inside_group) group_depths.emplace_back(group_depths.size(), -1);
			glyphs.emplace_back(Drawable{
				.position = common.position + glyph.offset * params.size / TextShaper::PixelsPerEm + line_offset,
				.color = common.color,
				.depth = common.depth,
				.outline_width = common.outline_width,
				.outline_color = common.outline_color,
			}, GlyphParams{
				.atlas_bounds = glyph.atlas_bounds,
				.size = params.size,
				.page = glyph.page,
			}, group_depths.size() - 1);
			group_depths.back().second = common.depth;
		}
	}
	return *this;
}

auto Renderer::Queue::to_primitive_list() const -> vector<Primitive>
{
	// Create a group remapping table so that the groups are sorted by depth
	sort(group_depths, [](auto const& a, auto const& b) {
		return a.second < b.second;
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
			.color = common.color,
			.outline_color = common.outline_color,
			.outline_width = common.outline_width,
			.rect_params = Primitive::RectParams{.size = rect.size},
		});
	}
	for (auto [common, circle, group]: circles) {
		primitives.emplace_back(Primitive{
			.type = Primitive::Type::Circle,
			.group_id = group_remapping[group],
			.position = common.position,
			.color = common.color,
			.outline_color = common.outline_color,
			.outline_width = common.outline_width,
			.circle_params = Primitive::CircleParams{.radius = circle.radius},
		});
	}
	for (auto [common, glyph, group]: glyphs) {
		primitives.emplace_back(Primitive{
			.type = Primitive::Type::Glyph,
			.group_id = group_remapping[group],
			.position = common.position,
			.color = common.color,
			.outline_color = common.outline_color,
			.outline_width = common.outline_width,
			.glyph_params = Primitive::GlyphParams{
				.atlas_bounds = glyph.atlas_bounds,
				.size = glyph.size,
				.page = glyph.page,
			},
		});
	}
	return primitives;
}

Renderer::Renderer(dev::Window& window, Logger::Category cat):
	cat{cat},
	gpu{window, cat},
	imgui{gpu},
	text_shaper{cat}
{
	auto& context = gpu.get_global_allocator().get_context();

	text_shaper.load_font("Mplus2"_id, globals::assets->get("Mplus2-Regular.ttf"_id), 500);
	text_shaper.load_font("Pretendard"_id, globals::assets->get("Pretendard-Regular.ttf"_id), 500);
	text_shaper.define_style("Sans-Regular"_id, {"Mplus2"_id, "Pretendard"_id}, 500);
	text_shaper.deserialize(globals::assets->get("font_atlas.zpp"_id));
	auto [new_atlas, atlas_upload] = lib::vuk::create_texture(gpu.get_global_allocator(), text_shaper.get_atlas(0), vuk::Format::eR8G8B8A8Unorm);
	auto compiler = lib::vuk::Compiler{};
	atlas_upload.as_released(lib::vuk::Access::eComputeSampled).wait(gpu.get_global_allocator(), compiler);
	static_atlas = move(new_atlas);

	lib::vuk::create_compute_pipeline(context, "worklist_gen", gpu::worklist_gen_spv);
	DEBUG_AS(cat, "Compiled worklist_gen pipeline");
	lib::vuk::create_compute_pipeline(context, "worklist_sort", gpu::worklist_sort_spv);
	DEBUG_AS(cat, "Compiled worklist_sort pipeline");
	lib::vuk::create_compute_pipeline(context, "draw_all", gpu::draw_all_spv);
	DEBUG_AS(cat, "Compiled draw_all pipeline");

	INFO_AS(cat, "Renderer initialized");
}

auto Renderer::prepare_text(TextStyle style, string_view text, optional<float> max_width) -> Text
{
	auto const style_id = [&] {
		switch (style) {
		case TextStyle::SansRegular: return "Sans-Regular"_id;
		};
	}();
	return text_shaper.shape(style_id, text, max_width.transform([&](auto w) { return w * TextShaper::PixelsPerEm; }));
}

auto Renderer::create_queue() -> Queue
{
	auto const transform = generate_transform(gpu.get_window().size(), gpu.get_window().scale());
	auto const inverse_transform = float4{
		-transform.x(),
		-transform.y(),
		1.0f / transform.z(),
		1.0f / transform.w(),
	};
	return Queue{transform, inverse_transform};
}

void Renderer::draw_frame(Queue&& queue)
{
	auto primitives = queue.to_primitive_list();
	gpu.frame([&, this](auto& allocator, auto&& target) -> lib::vuk::ManagedImage {
		// Update font atlas if needed
		auto atlas = lib::vuk::ManagedImage{};
		if (text_shaper.is_atlas_dirty()) {
			auto [new_atlas, atlas_upload] = lib::vuk::create_texture(gpu.get_global_allocator(), text_shaper.get_atlas(), vuk::Format::eR8G8B8A8Unorm);
			dynamic_atlas = move(new_atlas);
			atlas = move(atlas_upload);
		} else {
			atlas = lib::vuk::acquire_ia("atlas", dynamic_atlas.attachment, lib::vuk::Access::eComputeSampled);
		}

		auto next = lib::vuk::clear_image(move(target), {0.0f, 0.0f, 0.0f, 1.0f});
		if (!primitives.empty()) {
			auto [primitives_buf, worklists_buf, worklist_sizes_buf] = generate_worklists(gpu, allocator, primitives, queue.transform);
			next = draw_all(gpu, move(next), move(primitives_buf), move(worklists_buf), move(worklist_sizes_buf), static_atlas.view.get(), move(atlas));
		}
		return imgui.draw(allocator, move(next));
	});
}

}
