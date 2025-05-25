/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/gpu.cppm:
Initializes Vulkan and the rendergraph.
*/

module;
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
#include "macros/logger.hpp"

export module playnote.sys.gpu;

import playnote.preamble;
import playnote.config;
import playnote.logger;
import playnote.sys.window;

namespace playnote::sys {

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
	template<callable<ManagedImage(vuk::Allocator&, ManagedImage&&)> Func>
	void frame(Func&&);

	GPU(GPU const&) = delete;
	auto operator=(GPU const&) -> GPU& = delete;
	GPU(GPU&&) = delete;
	auto operator=(GPU&&) -> GPU& = delete;

private:
	Logger::Category* cat;

	class Instance {
	public:
		vkb::Instance instance;

		explicit Instance(Logger::Category*);
		~Instance();

		Instance(Instance const&) = delete;
		auto operator=(Instance const&) -> Instance& = delete;
		Instance(Instance&&) = delete;
		auto operator=(Instance&&) -> Instance& = delete;

	private:
		Logger::Category* cat;

		static auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
			VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const*,
			void*) -> VkBool32;
	};

	class Surface {
	public:
		VkSurfaceKHR surface;

		Surface(Logger::Category*, sys::Window&, Instance&);
		~Surface();

		Surface(Surface const&) = delete;
		auto operator=(Surface const&) -> Surface& = delete;
		Surface(Surface&&) = delete;
		auto operator=(Surface&&) -> Surface& = delete;

	private:
		Logger::Category* cat;
		Instance& instance;
	};

	class Device {
	public:
		vkb::Device device;

		Device(Logger::Category*, vkb::PhysicalDevice&);
		~Device();

		Device(Device const&) = delete;
		auto operator=(Device const&) -> Device& = delete;
		Device(Device&&) = delete;
		auto operator=(Device&&) -> Device& = delete;

	private:
		Logger::Category* cat;
	};

	struct Queues {
		VkQueue graphics;
		uint graphics_family_index;
		VkQueue transfer;
		uint transfer_family_index;
		VkQueue compute;
		uint compute_family_index;
	};

	// Helpers below use dumb types instead of RAII wrappers to avoid a linker bug
	// (the lambda types are distinct in different TUs when modules are in use)
	auto select_physical_device(Instance&, Surface&) -> vkb::PhysicalDevice;
	auto retrieve_queues(Device&) -> Queues;
	auto create_runtime(Instance&, VkPhysicalDevice, Device&, Queues const&) -> vuk::Runtime;
	auto create_swapchain(uvec2 size, vuk::Allocator&, Device&, Surface&, optional<vuk::Swapchain> old = nullopt) -> vuk::Swapchain;

	sys::Window& window;

	Instance instance;
	Surface surface;
	vkb::PhysicalDevice physical_device;
	Device device;
	vuk::Runtime runtime;
	vuk::DeviceSuperFrameResource global_resource;
	vuk::Allocator global_allocator;
	vuk::Swapchain swapchain;
	unique_ptr<vuk::extra::TracyContext> tracy_context;
};

GPU::GPU(sys::Window& window):
	// Beautiful, isn't it
	cat{globals::logger->register_category("Graphics", LogLevelGraphics)},
	window{window},
	instance{cat},
	surface{cat, window, instance},
	physical_device{select_physical_device(instance, surface)},
	device{cat, physical_device},
	runtime{create_runtime(instance, physical_device, device, retrieve_queues(device))},
	global_resource{runtime, FramesInFlight},
	global_allocator{global_resource},
	swapchain{create_swapchain(window.size(), global_allocator, device, surface)},
	tracy_context{vuk::extra::init_Tracy(global_allocator)}
{
	INFO_AS(cat, "Vulkan initialized");
}

template<callable<ManagedImage(vuk::Allocator&, ManagedImage&&)> Func>
void GPU::frame(Func&& func)
{
	auto& frame_resource = global_resource.get_next_frame();
	auto frame_allocator = vuk::Allocator{frame_resource};
	runtime.next_frame();
	auto imported_swapchain = vuk::acquire_swapchain(swapchain);
	auto swapchain_image = vuk::acquire_next_image("swp_img", move(imported_swapchain));

	auto result = func(frame_allocator, std::move(swapchain_image));

	auto entire_thing = vuk::enqueue_presentation(std::move(result));
	auto compiler = vuk::Compiler{};
	auto profiling_cbs = vuk::extra::make_Tracy_callbacks(*tracy_context);
	entire_thing.submit(frame_allocator, compiler, { .callbacks = profiling_cbs });
}

}
