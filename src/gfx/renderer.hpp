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

	struct Circle {
		vec2 position;
		vec2 velocity;
		vec4 color;
		float radius;
		float _pad0[3];
	};
	static_assert(alignof(Circle) == sizeof(float));

	// An accumulator of primitives to draw.
	class Queue {
	public:
		// Add a solid color rectangle to the draw queue.
		auto enqueue_rect(id layer, Rect) -> Queue&;

		auto add_circle_blur(Circle) -> Queue&;
		auto add_circle_aa(Circle) -> Queue&;

	private:
		struct Layer {
			vector<Rect> rects;
		};

		friend Renderer;
		unordered_map<id, Layer> layers;
		vector<Circle> circles_blur;
		vector<Circle> circles_aa;
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
	[[nodiscard]] auto draw_circles_blur(lib::vuk::Allocator&, lib::vuk::ManagedImage&&,
		span<Circle const>) -> lib::vuk::ManagedImage;
	[[nodiscard]] auto draw_circles_aa(lib::vuk::Allocator&, lib::vuk::ManagedImage&&,
		span<Circle const>) -> lib::vuk::ManagedImage;
	[[nodiscard]] auto correct_gamma(lib::vuk::Allocator&, lib::vuk::ManagedImage&&) -> lib::vuk::ManagedImage;
};

inline auto srgb_decode(vec4 color) -> vec4
{
	float r = color.r() < 0.04045 ? (1.0 / 12.92) * color.r() : pow((color.r() + 0.055) * (1.0 / 1.055), 2.4);
	float g = color.g() < 0.04045 ? (1.0 / 12.92) * color.g() : pow((color.g() + 0.055) * (1.0 / 1.055), 2.4);
	float b = color.b() < 0.04045 ? (1.0 / 12.92) * color.b() : pow((color.b() + 0.055) * (1.0 / 1.055), 2.4);
	return {r, g, b, color.a()};
}

inline auto Renderer::Queue::enqueue_rect(id layer, Rect rect) -> Queue&
{
	rect.color = srgb_decode(rect.color);
	layers[layer].rects.emplace_back(rect);
	return *this;
}

inline auto Renderer::Queue::add_circle_blur(Circle circle) -> Queue&
{
	circle.color = srgb_decode(circle.color);
	circles_blur.emplace_back(circle);
	return *this;
}

inline auto Renderer::Queue::add_circle_aa(Circle circle) -> Queue&
{
	circle.color = srgb_decode(circle.color);
	circles_aa.emplace_back(circle);
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

#include "spv/gamma.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "gamma", gamma_spv);
	DEBUG_AS(cat, "Compiled gamma pipeline");

#include "spv/circles_blur.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "circles_blur", circles_blur_spv);
#include "spv/circles_aa.slang.spv.h"
	lib::vuk::create_compute_pipeline(context, "circles_aa", circles_aa_spv);
	DEBUG_AS(cat, "Compiled circles pipeline");

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
		if (!queue.circles_blur.empty())
			next = draw_circles_blur(allocator, move(next), queue.circles_blur);
		if (!queue.circles_aa.empty())
			next = draw_circles_aa(allocator, move(next), queue.circles_aa);
		next = correct_gamma(allocator, move(next));
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

inline auto Renderer::draw_circles_blur(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest,
		span<Circle const> circles) -> lib::vuk::ManagedImage
{
	auto circles_buf = lib::vuk::create_scratch_buffer(allocator, circles);
	auto pass = lib::vuk::make_pass("circles_blur",
		[window_size = gpu.get_window().size(), window_scale = gpu.get_window().scale(), circles_buf, circles_count = circles.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eComputeRW) target)
	{
		struct Push {
			uint32 circles_count;
			float timer;
		};
		lib::vuk::set_cmd_defaults(cmd)
			.bind_compute_pipeline("circles_blur")
			.bind_buffer(0, 0, circles_buf)
			.bind_image(0, 1, target)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, Push{
				.circles_count = circles_count,
				.timer = ratio(globals::glfw->get_time(), 1s),
			})
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, window_scale)
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest));
}

inline auto Renderer::draw_circles_aa(lib::vuk::Allocator& allocator, lib::vuk::ManagedImage&& dest,
		span<Circle const> circles) -> lib::vuk::ManagedImage
{
	auto circles_buf = lib::vuk::create_scratch_buffer(allocator, circles);
	auto pass = lib::vuk::make_pass("circles_aa",
		[window_size = gpu.get_window().size(), window_scale = gpu.get_window().scale(), circles_buf, circles_count = circles.size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eComputeRW) target)
	{
		struct Push {
			uint32 circles_count;
			float timer;
		};
		lib::vuk::set_cmd_defaults(cmd)
			.bind_compute_pipeline("circles_aa")
			.bind_buffer(0, 0, circles_buf)
			.bind_image(0, 1, target)
			.push_constants(lib::vuk::ShaderStageFlagBits::eCompute, 0, Push{
				.circles_count = circles_count,
				.timer = ratio(globals::glfw->get_time(), 1s),
			})
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.specialize_constants(2, window_scale)
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest));
}

inline auto Renderer::correct_gamma(lib::vuk::Allocator& allocator,
	lib::vuk::ManagedImage&& dest) -> lib::vuk::ManagedImage
{
	auto pass = lib::vuk::make_pass("gamma",
		[window_size = gpu.get_window().size()]
		(lib::vuk::CommandBuffer& cmd, VUK_IA(lib::vuk::Access::eComputeRW) target)
	{
		lib::vuk::set_cmd_defaults(cmd)
			.bind_compute_pipeline("gamma")
			.bind_image(0, 0, target)
			.specialize_constants(0, window_size.x()).specialize_constants(1, window_size.y())
			.dispatch_invocations(window_size.x(), window_size.y(), 1);
		return target;
	});
	return pass(move(dest));
}

}
