/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/vuk.hpp:
Wrapper for vuk, a Vulkan rendergraph library.
*/

#pragma once
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/CommandBuffer.hpp" // Required to work around bug in TracyIntegration.hpp
#include "vuk/extra/TracyIntegration.hpp"
#include "vuk/vsl/Core.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Value.hpp"
#include "preamble.hpp"
#include "lib/vulkan.hpp"

namespace playnote::lib::vuk {

using namespace ::vuk;

// A shorthand for an image attachment currently being managed by vuk's rendergraph.
using ManagedImage = Value<ImageAttachment>;

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
	optional<Swapchain> old = nullopt) -> Swapchain;

// Shorthand for vuk's Tracy integration resources.
using TracyContext = std::unique_ptr<extra::TracyContext>;

// Create the resources for vuk's Tracy integration.
[[nodiscard]] auto create_tracy_context(Allocator& allocator) -> TracyContext;

// Start a new frame and create its single-frame allocator.
// Throws if vuk throws.
auto begin_frame(Runtime& runtime, GlobalResource& resource) -> Allocator;

// Retrieve and acquire the next image in the swapchain.
// Throws if vuk throws.
[[nodiscard]] auto acquire_swapchain_image(Swapchain& swapchain, string_view name) -> ManagedImage;

// Submit the given image for presentation on the swapchain surface.
// Throws if vuk throws.
void submit(Allocator& allocator, TracyContext const& tracy_context, ManagedImage&& image);

// Compile a vertex and fragment shader pair into a graphics pipeline.
// Throws if vuk throws.
template<usize NVert, usize NFrag>
void create_graphics_pipeline(Runtime& runtime, string_view name,
	array<uint32, NVert> const& vertex_shader, array<uint32, NFrag> const& fragment_shader)
{
	auto const vert_name = format("{}.vert", name);
	auto const frag_name = format("{}.frag", name);
	auto pci = PipelineBaseCreateInfo{};
	pci.add_static_spirv(vertex_shader.data(), NVert, move(vert_name));
	pci.add_static_spirv(fragment_shader.data(), NFrag, move(frag_name));
	runtime.create_named_pipeline(name, pci);
}

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
