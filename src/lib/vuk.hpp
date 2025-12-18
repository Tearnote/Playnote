/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/vk/VkSwapchain.hpp" // IWYU pragma: export
#include "vuk/runtime/vk/Allocator.hpp" // IWYU pragma: export
#include "vuk/runtime/vk/VkRuntime.hpp" // IWYU pragma: export
#include "vuk/runtime/CommandBuffer.hpp"
#include "vuk/vsl/Core.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/Buffer.hpp"
#include "vuk/Types.hpp"
#include "vuk/Value.hpp"
#include "preamble.hpp"
#include "lib/vulkan.hpp"

// vuk interface imports
#include "vuk/vsl/Core.hpp" // IWYU pragma: export
#include "vuk/RenderGraph.hpp" // IWYU pragma: export

namespace playnote::lib::vuk {

using namespace ::vuk;

// A shorthand for an image attachment currently being managed by vuk's rendergraph.
using ManagedImage = Value<ImageAttachment>;

// A shorthand for a buffer currently being managed by vuk's rendergraph.
using ManagedBuffer = Value<Buffer>;

// An owned, long-lived image.
struct Texture {
	Unique<Image> image;
	Unique<ImageView> view;
	ImageAttachment attachment;
};

// Initialize vuk by building a Runtime object.
// Throws if vuk throws.
auto create_runtime(vk::Instance instance, vk::Device device, vk::QueueSet const& queues) -> Runtime;

// An allocator resource providing memory for objects that span multiple frames, and is the parent
// resource for single-frame resources.
using GlobalResource = DeviceSuperFrameResource;

// Create a swapchain object from a device's surface. The swapchain image is RGB8 Unorm, non-linear.
// FIFO presentation mode is used. Actual image count might be larger than requested.
// Throws runtime_error on failure, or if vuk throws.
[[nodiscard]] auto create_swapchain(Allocator& allocator, vk::Device device, int2 size,
	int image_count, optional<Swapchain> old = nullopt) -> Swapchain;

// Start a new frame and create its single-frame allocator.
// Throws if vuk throws.
auto begin_frame(Runtime& runtime, GlobalResource& resource) -> Allocator;

// Retrieve and acquire the next image in the swapchain.
// Throws if vuk throws.
[[nodiscard]] auto acquire_swapchain_image(Swapchain& swapchain, string_view name) -> ManagedImage;

// Submit the given image for presentation on the swapchain surface.
// Throws if vuk throws.
void submit(Allocator& allocator, ManagedImage&& image);

// Compile a combined vertex and fragment shader into a graphics pipeline. Expects "vertexMain"
// and "fragmentMain" entry points.
// Throws if vuk throws.
void create_graphics_pipeline(Runtime& runtime, string_view name, span<uint const> shader);

// Compile a compute shader into a compute pipeline. Expects "computeMain" entry point.
// Throws if vuk throws.
void create_compute_pipeline(Runtime& runtime, string_view name, span<uint const> shader);

// Clear an image with a solid color.
// Throws if vuk throws.
auto clear_image(ManagedImage&& input, float4 color) -> ManagedImage;

// Create a host-visible buffer with the provided data. Memory is never freed, so use with
// a frame allocator.
// Throws if vuk throws.
template<typename T>
auto create_scratch_buffer(Allocator& allocator, span<T> data) -> Buffer
{
	auto [buf, fut] = create_buffer(allocator, MemoryUsage::eCPUtoGPU, DomainFlagBits::eTransferOnGraphics, data);
	return buf.release();
}

// Create a GPU-only buffer with the provided data. The returned value is a future of the data
// upload. Memory is never freed, so use with a frame allocator.
// Throws if vuk throws.
template<typename T>
auto create_gpu_buffer(Allocator& allocator, span<T> data) -> ManagedBuffer
{
	auto [buf, fut] = create_buffer(allocator, MemoryUsage::eGPUonly, DomainFlagBits::eTransferOnGraphics, data);
	buf.release();
	return fut;
}

// Create a GPU-only 2D image with the provided data. Returns the owned image resource and the future
// of the data upload. The multi-array dimensions are used as texture dimensions, with the 3rd
// dimension being the channel count. Format must match the data type and channel count; this is
// not checked.
// Throws if vuk throws.
template<typename T>
auto create_texture(Allocator& allocator, const_multi_array_ref<T, 3> data, Format format) -> pair<Texture, ManagedImage>
{
	auto ia = ImageAttachment{
		.usage = ImageUsageFlagBits::eSampled | ImageUsageFlagBits::eTransferDst,
		.extent = {static_cast<uint>(data.shape()[0]), static_cast<uint>(data.shape()[1]), 1},
		.format = format,
		.sample_count = Samples::e1,
		.view_type = ImageViewType::e2D,
		.base_level = 0,
		.level_count = 1,
		.base_layer = 0,
		.layer_count = 1,
	};
	auto image = allocate_image(allocator, ia);
	ia.image = **image;
	auto view = allocate_image_view(allocator, ia);
	ia.image_view = **view;
	auto result = Texture{
		.image = move(*image),
		.view = move(*view),
		.attachment = ia,
	};
	return {move(result), host_data_to_image(allocator, DomainFlagBits::eTransferOnTransfer, ia, data.data())};
}

// Set the default command buffer configuration used by this application.
// Throws if vuk throws.
auto set_cmd_defaults(CommandBuffer& cmd) -> CommandBuffer&;

}
