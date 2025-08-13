/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/vuk.cpp:
Implementation file for lib/vuk.hpp.
*/

#include "lib/vuk.hpp"

#include "vuk/runtime/ThisThreadExecutor.hpp"
#include "VkBootstrap.h"
#include "preamble.hpp"

namespace playnote::lib::vuk {

auto create_runtime(vk::Instance instance, vk::Device device, vk::QueueSet const& queues) -> Runtime
{
	auto const pointers = FunctionPointers{
		vkGetInstanceProcAddr,
		vkGetDeviceProcAddr,
// Please don't hate me martty, it saves me so much typing
// (and of course we won't spend the extra 1ms it would take to load the functions a second time)
#define VUK_X(name) name,
#define VUK_Y(name) name,
#include "vuk/runtime/vk/VkPFNOptional.hpp"
#include "vuk/runtime/vk/VkPFNRequired.hpp"
	};

	auto executors = std::vector<std::unique_ptr<Executor>>{};
	executors.reserve(4);
	executors.emplace_back(std::make_unique<ThisThreadExecutor>());
	executors.emplace_back(create_vkqueue_executor(pointers, device->device, queues.graphics,
		queues.graphics_family_index, DomainFlagBits::eGraphicsQueue));
	if (queues.compute) {
		executors.emplace_back(create_vkqueue_executor(pointers, device->device, queues.compute,
			queues.compute_family_index, DomainFlagBits::eComputeQueue));
	}
	if (queues.transfer) {
		executors.emplace_back(create_vkqueue_executor(pointers, device->device,
			queues.transfer, queues.transfer_family_index, DomainFlagBits::eTransferQueue));
	}

	return Runtime{RuntimeCreateParameters{
		.instance = *instance,
		.device = device->device,
		.physical_device = device->physical_device,
		.executors = move(executors),
		.pointers = pointers,
	}};
}

[[nodiscard]] auto create_swapchain(Allocator& allocator, vk::Device device, uvec2 size,
	optional<Swapchain> old) -> Swapchain
{
	auto vkbswapchain_result = vkb::SwapchainBuilder{*device}
		.set_old_swapchain(old? old->swapchain : VK_NULL_HANDLE)
		.set_desired_extent(size.x(), size.y())
		.set_desired_format({
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		})
		.add_fallback_format({
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		})
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_image_usage_flags(
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		)
		.build();
	if (!vkbswapchain_result)
		throw runtime_error_fmt("Failed to create the swapchain: {}",
			vkbswapchain_result.error().message());
	auto vkbswapchain = vkbswapchain_result.value();

	if (old) {
		allocator.deallocate(span{&old->swapchain, 1});
		for (auto& image: old->images)
			allocator.deallocate(span{&image.image_view, 1});
	}

	auto swapchain = Swapchain{allocator, vkbswapchain.image_count};

	for (auto [image, view]: views::zip(*vkbswapchain.get_images(),
		     *vkbswapchain.get_image_views())) {
		swapchain.images.emplace_back(ImageAttachment{
			.image = Image{image, nullptr},
			.image_view = ImageView{{}, view},
			.extent = {vkbswapchain.extent.width, vkbswapchain.extent.height, 1},
			.format = static_cast<Format>(vkbswapchain.image_format),
			.sample_count = Samples::e1,
			.view_type = ImageViewType::e2D,
			.base_level = 0,
			.level_count = 1,
			.base_layer = 0,
			.layer_count = 1,
		});
	}

	swapchain.swapchain = vkbswapchain.swapchain;
	swapchain.surface = device->surface;
	return swapchain;
}

[[nodiscard]] auto create_tracy_context(Allocator& allocator) -> TracyContext
{
	return extra::init_Tracy(allocator);
}

auto begin_frame(Runtime& runtime, GlobalResource& resource) -> Allocator
{
	auto& frame_resource = resource.get_next_frame();
	auto frame_allocator = Allocator{frame_resource};
	runtime.next_frame();
	return frame_allocator;
}

[[nodiscard]] auto acquire_swapchain_image(Swapchain& swapchain,
	string_view name) -> ManagedImage
{
	auto acquired_swapchain = acquire_swapchain(swapchain);
	return acquire_next_image(name, move(acquired_swapchain));
}

void submit(Allocator& allocator, TracyContext const& tracy_context, ManagedImage&& image)
{
	auto entire_thing = enqueue_presentation(move(image));
	auto compiler = Compiler{};
	auto const profiling_cbs = extra::make_Tracy_callbacks(*tracy_context);
	entire_thing.submit(allocator, compiler, {.callbacks = profiling_cbs});
}

auto clear_image(ManagedImage&& input, vec4 color) -> ManagedImage
{
	return clear_image(move(input), ClearColor{color.r(), color.g(), color.b(), color.a()});
}

auto set_cmd_defaults(CommandBuffer& cmd) -> CommandBuffer&
{
	return cmd
		.set_dynamic_state(DynamicStateFlagBits::eScissor | DynamicStateFlagBits::eViewport)
		.set_viewport(0, Rect2D::framebuffer())
		.set_scissor(0, Rect2D::framebuffer())
		.set_rasterization({})
		.broadcast_color_blend({});
}

}
