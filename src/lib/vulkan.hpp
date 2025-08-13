/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/vulkan.hpp:
Imports of Vulkan and helper libraries.
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
#include "logger.hpp"

// Forward declarations

struct VkSurfaceKHR_T;
struct VkInstance_T;
struct VkQueue_T;

namespace vkb {
struct Instance;
struct PhysicalDevice;
struct Device;
};

namespace playnote::lib::vk {

// A Vulkan instance, representing library context.
using Instance = vkb::Instance*;

// Vulkan instance handle; identical to VkInstance.
using RawInstance = VkInstance_T*;

// Create a Vulkan instance.
// Throws runtime_error on failure.
auto create_instance(string_view name, Logger::Category* debug_logger) -> Instance;

// Destroy the Vulkan instance.
void destroy_instance(Instance&& instance) noexcept;

// Retrieve the raw instance handle.
auto get_raw_instance(Instance instance) noexcept -> RawInstance;

// A window's surface, as visible for Vulkan as a drawing target.
using Surface = VkSurfaceKHR_T*;

// Surface creation is handled by lib.window

// Destroy the window's Vulkan surface.
void destroy_surface(Instance instance, Surface surface) noexcept;

namespace detail {

struct PhysicalDeviceDeleter {
	static void operator()(vkb::PhysicalDevice*) noexcept;
};

}

// A specific Vulkan-compatible GPU on the system.
using PhysicalDevice = unique_ptr<vkb::PhysicalDevice, detail::PhysicalDeviceDeleter>;

// Choose and return the best available GPU.
// Throws runtime_error if none is found.
auto select_physical_device(Instance const& instance, Surface surface) -> PhysicalDevice;

// Return the driver version triple in use by a specific GPU.
auto get_driver_version(PhysicalDevice const& physical_device) -> array<uint32, 3>;

// Return a GPU's name as reported by the driver.
auto get_device_name(PhysicalDevice const& physical_device) -> string_view;

// A logical Vulkan device created from a physical one.
using Device = vkb::Device*;

// Create a Vulkan device out of a physical device.
// Throws runtime_error on failure.
auto create_device(PhysicalDevice const& physical_device) -> Device;

// Destroy the Vulkan device. Make sure it's idle first.
void destroy_device(Device&& device) noexcept;

// A queue into which GPU commands can be submitted.
using Queue = VkQueue_T*;

// Set of queues available on a device. A queue is nullptr if unavailable.
struct QueueSet {
	Queue graphics;
	uint32 graphics_family_index;
	Queue transfer;
	uint32 transfer_family_index;
	Queue compute;
	uint32 compute_family_index;
};

// Fill in a QueueSet from a Vulkan device.
auto retrieve_device_queues(Device device) -> QueueSet;

// A shorthand for an image attachment currently being managed by vuk's rendergraph.
using ManagedImage = vuk::Value<vuk::ImageAttachment>;

// A set of Vulkan object references used by vuk.
using Runtime = vuk::Runtime;

// Initialize vuk by building a Runtime object.
// Throws if vuk throws.
auto create_runtime(Instance instance, Device device, QueueSet const& queues) -> Runtime;

// An allocator resource providing memory for objects that span multiple frames, and is the parent
// resource for single-frame resources.
using GlobalResource = vuk::DeviceSuperFrameResource;

// An interface for creating Vulkan objects out of a resource.
using Allocator = vuk::Allocator;

// An encapsulation of window surface and related access objects.
using Swapchain = vuk::Swapchain;

// Create a swapchain object from a device's surface. The swapchain image is RGB8 Unorm, non-linear.
// FIFO presentation mode is used.
// Throws runtime_error on failure, or if vuk throws.
[[nodiscard]] auto create_swapchain(Allocator& allocator, Device device, uvec2 size,
	optional<Swapchain> old = nullopt) -> Swapchain;

// Shorthand for vuk's Tracy integration resources.
using TracyContext = std::unique_ptr<vuk::extra::TracyContext>;

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
	auto pci = vuk::PipelineBaseCreateInfo{};
	pci.add_static_spirv(vertex_shader.data(), NVert, move(vert_name));
	pci.add_static_spirv(fragment_shader.data(), NFrag, move(frag_name));
	runtime.create_named_pipeline(name, pci);
}

// Clear an image with a solid color.
// Throws if vuk throws.
auto clear_image(ManagedImage&& input, vec4 color) -> ManagedImage;

// Rendergraph support

using vuk::make_pass;
using vuk::CommandBuffer;
using vuk::Access;
using vuk::Buffer;

// Create a host-visible buffer with the provided data. Memory is never freed, so use with
// a frame allocator.
// Throws if vuk throws.
template<typename T>
auto create_scratch_buffer(Allocator& allocator, span<T> data) -> Buffer
{
	auto [buf, fut] = vuk::create_buffer(allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, data);
	return buf.release();
}

// Set the default command buffer configuration used by this application.
// Throws if vuk throws.
auto set_cmd_defaults(CommandBuffer& cmd) -> CommandBuffer&;

}
