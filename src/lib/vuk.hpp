/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/vuk.hpp:
Wrapper for vuk, a Vulkan rendergraph library.
*/

#pragma once
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/vk/VkSwapchain.hpp" // IWYU pragma: export
#include "vuk/runtime/vk/Allocator.hpp" // IWYU pragma: export
#include "vuk/runtime/vk/VkRuntime.hpp" // IWYU pragma: export
#include "vuk/runtime/CommandBuffer.hpp"
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

// Swapchain presentation mode.
enum class PresentMode {
	Immediate = VK_PRESENT_MODE_IMMEDIATE_KHR,
	Mailbox = VK_PRESENT_MODE_MAILBOX_KHR,
	Fifo = VK_PRESENT_MODE_FIFO_KHR,
};

// Initialize vuk by building a Runtime object.
// Throws if vuk throws.
auto create_runtime(vk::Instance instance, vk::Device device, vk::QueueSet const& queues) -> Runtime;

// An allocator resource providing memory for objects that span multiple frames, and is the parent
// resource for single-frame resources.
using GlobalResource = DeviceSuperFrameResource;

// Create a swapchain object from a device's surface. The swapchain image is RGB8 Unorm, non-linear.
// FIFO presentation mode is used.
// Throws runtime_error on failure, or if vuk throws.
[[nodiscard]] auto create_swapchain(Allocator& allocator, vk::Device device, uvec2 size,
	PresentMode, optional<Swapchain> old = nullopt) -> Swapchain;

// Start a new frame and create its single-frame allocator.
// Throws if vuk throws.
auto begin_frame(Runtime& runtime, GlobalResource& resource) -> Allocator;

// Retrieve and acquire the next image in the swapchain.
// Throws if vuk throws.
[[nodiscard]] auto acquire_swapchain_image(Swapchain& swapchain, string_view name) -> ManagedImage;

// Submit the given image for presentation on the swapchain surface.
// Throws if vuk throws.
void submit(Allocator& allocator, ManagedImage&& image);

// Compile a vertex and fragment shader pair into a graphics pipeline.
// Throws if vuk throws.
void create_graphics_pipeline(Runtime& runtime, string_view name,
	span<uint32 const> vertex_shader, span<uint32 const> fragment_shader);

// Clear an image with a solid color.
// Throws if vuk throws.
auto clear_image(ManagedImage&& input, vec4 color) -> ManagedImage;

// Create a host-visible buffer with the provided data. Memory is never freed, so use with
// a frame allocator.
// Throws if vuk throws.
template<typename T>
auto create_scratch_buffer(Allocator& allocator, span<T> data) -> Buffer
{
	auto [buf, fut] = create_buffer(allocator, MemoryUsage::eCPUtoGPU, DomainFlagBits::eTransferOnGraphics, data);
	return buf.release();
}

// Set the default command buffer configuration used by this application.
// Throws if vuk throws.
auto set_cmd_defaults(CommandBuffer& cmd) -> CommandBuffer&;

}
