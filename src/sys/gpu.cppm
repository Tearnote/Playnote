/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/gpu.cppm:
Initializes Vulkan and the rendergraph.
*/

module;
#include <optional>
#include <utility>
#include <memory>
#include "libassert/assert.hpp"
#include "volk.h"
#include "VkBootstrap.h"
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/vk/VkSwapchain.hpp"
#include "vuk/runtime/vk/Allocator.hpp"
#include "vuk/runtime/vk/VkRuntime.hpp"
#include "vuk/runtime/CommandBuffer.hpp" // Required to work around bug in TracyIntegration.hpp
#include "vuk/extra/TracyIntegration.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Value.hpp"
#include "vuk/Types.hpp"
#include "util/log_macros.hpp"
#include "config.hpp"

export module playnote.sys.gpu;

import playnote.preamble;
import playnote.stx.callable;
import playnote.stx.except;
import playnote.stx.math;
import playnote.util.raii;
import playnote.sys.window;
import playnote.globals;

namespace playnote::sys {

using stx::uvec2;

// Saves some typing
export using ManagedImage = vuk::Value<vuk::ImageAttachment>;

// RAII encapsulation of GPU state, handling initialization and frame preparation/presentation
export class GPU {
public:
	static constexpr auto FramesInFlight = 2u; // 2 or 3, low latency vs smoothness

	explicit GPU(sys::Window&);
	~GPU() { runtime.wait_idle(); }

	auto get_window() -> sys::Window& { return window; }
	auto get_global_allocator() -> vuk::Allocator& { return global_allocator; }

	// Prepare and present a single frame
	// All vuk draw commands must be submitted within the callback
	// The callback is provided with the frame allocator and swapchain image
	template<stx::callable<ManagedImage(vuk::Allocator&, ManagedImage&&)> Func>
	void frame(Func&&);

	GPU(GPU const&) = delete;
	auto operator=(GPU const&) -> GPU& = delete;
	GPU(GPU&&) = delete;
	auto operator=(GPU&&) -> GPU& = delete;

private:
	using Instance = util::RAIIResource<vkb::Instance, decltype([](auto& i) {
		vkb::destroy_instance(i);
		L_DEBUG("Vulkan instance cleaned up");
	})>;

	struct Surface_impl {
		VkSurfaceKHR surface;
		vkb::Instance& instance;
		operator VkSurfaceKHR() { return surface; }
	};

	using Surface = util::RAIIResource<Surface_impl, decltype([](auto& s) {
		vkDestroySurfaceKHR(s.instance, s.surface, nullptr);
		L_DEBUG("Vulkan surface cleaned up");
	})>;
	using Device = util::RAIIResource<vkb::Device, decltype([](auto& d) {
		vkb::destroy_device(d);
		L_DEBUG("Vulkan device cleaned up");
	})>;

	struct Queues {
		VkQueue graphics;
		uint graphics_family_index;
		VkQueue transfer;
		uint transfer_family_index;
		VkQueue compute;
		uint compute_family_index;
	};

#ifdef VK_VALIDATION
	// Forward Vulkan validation errors to the logger
	static auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
		VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const*,
		void*) -> VkBool32;
#endif
	// Helpers below use dumb types instead of RAII wrappers to avoid a linker bug
	// (the lambda types are distinct in different TUs when modules are in use)
	auto create_instance() -> vkb::Instance;
	auto create_surface(vkb::Instance&) -> Surface_impl;
	auto select_physical_device(vkb::Instance&, VkSurfaceKHR) -> vkb::PhysicalDevice;
	auto create_device(vkb::PhysicalDevice&) -> vkb::Device;
	auto retrieve_queues(vkb::Device&) -> Queues;
	auto create_runtime(VkInstance, VkPhysicalDevice, VkDevice, Queues const&) -> vuk::Runtime;
	auto create_swapchain(uvec2 size, vuk::Allocator&, vkb::Device&, Surface_impl&, std::optional<vuk::Swapchain> old = std::nullopt) -> vuk::Swapchain;

	sys::Window& window;

	Instance instance{};
	Surface surface{};
	vkb::PhysicalDevice physical_device{};
	Device device{};
	vuk::Runtime runtime;
	vuk::DeviceSuperFrameResource global_resource;
	vuk::Allocator global_allocator;
	vuk::Swapchain swapchain;
	std::unique_ptr<vuk::extra::TracyContext> tracy_context;
};

GPU::GPU(sys::Window& window):
	// Beautiful, isn't it
	window{window},
	instance{create_instance()},
	surface{create_surface(*instance)},
	physical_device{select_physical_device(*instance, *surface)},
	device{create_device(physical_device)},
	runtime{create_runtime(*instance, physical_device, *device, retrieve_queues(*device))},
	global_resource{runtime, FramesInFlight},
	global_allocator{global_resource},
	swapchain{create_swapchain(window.size(), global_allocator, *device, *surface)},
	tracy_context{vuk::extra::init_Tracy(global_allocator)}
{
	L_INFO("Vulkan initialized");
}

template<stx::callable<ManagedImage(vuk::Allocator&, ManagedImage&&)> Func>
void GPU::frame(Func&& func)
{
	auto& frame_resource = global_resource.get_next_frame();
	auto frame_allocator = vuk::Allocator{frame_resource};
	runtime.next_frame();
	auto imported_swapchain = vuk::acquire_swapchain(swapchain);
	auto swapchain_image = vuk::acquire_next_image("swp_img", std::move(imported_swapchain));

	auto result = func(frame_allocator, std::move(swapchain_image));

	auto entire_thing = vuk::enqueue_presentation(std::move(result));
	auto compiler = vuk::Compiler{};
	auto profiling_cbs = vuk::extra::make_Tracy_callbacks(*tracy_context);
	entire_thing.submit(frame_allocator, compiler, { .callbacks = profiling_cbs });
}

}
