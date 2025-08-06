/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/gpu.cppm:
Initializes Vulkan and the rendergraph.
*/

module;
#include "macros/assert.hpp"
#include "preamble.hpp"
#include "config.hpp"
#include "logger.hpp"

export module playnote.dev.gpu;

import playnote.lib.vulkan;
import playnote.dev.window;

namespace playnote::dev {

namespace vk = lib::vk;

export using vk::ManagedImage;

// RAII encapsulation of GPU state, handling initialization and frame preparation/presentation
export class GPU {
public:
	static constexpr auto FramesInFlight = 2u; // 2 or 3, low latency vs smoothness

	explicit GPU(dev::Window&);
	~GPU() { runtime.wait_idle(); }

	[[nodiscard]] auto get_window() const -> dev::Window& { return window; }
	[[nodiscard]] auto get_global_allocator() -> vk::Allocator& { return global_allocator; }

	// Prepare and present a single frame. All vuk draw commands must be submitted within
	// the callback. The callback is provided with the frame allocator and swapchain image.
	template<callable<ManagedImage(vk::Allocator&, ManagedImage&&)> Func>
	void frame(Func&&);

	GPU(GPU const&) = delete;
	auto operator=(GPU const&) -> GPU& = delete;
	GPU(GPU&&) = delete;
	auto operator=(GPU&&) -> GPU& = delete;

private:
	InstanceLimit<GPU, 1> instance_limit;
	Logger::Category* cat;

	// RAII wrapper of a Vulkan instance.
	class Instance {
	public:
		vk::Instance instance;

		explicit Instance(Logger::Category*);
		~Instance() noexcept;

		Instance(Instance const&) = delete;
		auto operator=(Instance const&) -> Instance& = delete;
		Instance(Instance&&) = delete;
		auto operator=(Instance&&) -> Instance& = delete;

	private:
		Logger::Category* cat;
	};

	// RAII wrapper of a Vulkan surface.
	class Surface {
	public:
		vk::Surface surface;

		Surface(Logger::Category*, dev::Window&, Instance&);
		~Surface() noexcept;

		Surface(Surface const&) = delete;
		auto operator=(Surface const&) -> Surface& = delete;
		Surface(Surface&&) = delete;
		auto operator=(Surface&&) -> Surface& = delete;

	private:
		Logger::Category* cat;
		Instance& instance;
	};

	// RAII wrapper of a Vulkan device.
	class Device {
	public:
		vk::Device device;

		Device(Logger::Category*, vk::PhysicalDevice const&);
		~Device() noexcept;

		Device(Device const&) = delete;
		auto operator=(Device const&) -> Device& = delete;
		Device(Device&&) = delete;
		auto operator=(Device&&) -> Device& = delete;

	private:
		Logger::Category* cat;
	};

	// Logging wrappers.
	[[nodiscard]] auto select_physical_device(Instance const&, Surface const&) const -> vk::PhysicalDevice;
	auto create_swapchain(vk::Allocator& allocator, Device& device, uvec2 size,
		optional<vk::Swapchain> old = nullopt) const -> vk::Swapchain;

	dev::Window& window;

	Instance instance;
	Surface surface;
	vk::PhysicalDevice physical_device;
	Device device;
	vk::Runtime runtime;
	vk::GlobalResource global_resource;
	vk::Allocator global_allocator;
	vk::Swapchain swapchain;
	vk::TracyContext tracy_context;
};

GPU::Instance::Instance(Logger::Category* cat):
	instance{vk::create_instance(AppTitle, cat)},
	cat{cat}
{ DEBUG_AS(cat, "Vulkan instance created"); }

GPU::Instance::~Instance() noexcept
{
	vk::destroy_instance(instance);
	DEBUG_AS(cat, "Vulkan instance cleaned up");
}

GPU::Surface::Surface(Logger::Category* cat, dev::Window& window, Instance& instance):
	surface{window.create_surface(instance.instance)},
	cat{cat},
	instance{instance} {}

GPU::Surface::~Surface() noexcept
{
	vk::destroy_surface(instance.instance, surface);
	DEBUG_AS(cat, "Vulkan surface cleaned up");
}

GPU::Device::Device(Logger::Category* cat, vk::PhysicalDevice const& physical_device):
	device{vk::create_device(physical_device)},
	cat{cat}
{ DEBUG_AS(cat, "Vulkan device created"); }

GPU::Device::~Device() noexcept
{
	vk::destroy_device(device);
	DEBUG_AS(cat, "Vulkan device cleaned up");
}

[[nodiscard]] auto GPU::select_physical_device(Instance const& instance, Surface const& surface) const -> vk::PhysicalDevice
{
	auto physical_device = vk::select_physical_device(instance.instance, surface.surface);
	auto const version = vk::get_driver_version(physical_device);

	INFO_AS(cat, "GPU selected: {}", physical_device.properties.deviceName);
	DEBUG_AS(cat, "Vulkan driver version {}.{}.{}", version[0], version[1], version[2]);
	return physical_device;
}

auto GPU::create_swapchain(vk::Allocator& allocator, Device& device, uvec2 size,
	optional<vk::Swapchain> old) const -> vk::Swapchain
{
	auto const recreating = old.has_value();
	auto swapchain = vk::create_swapchain(allocator, device.device, size, move(old));
	if (!recreating)
		DEBUG_AS(cat, "Created swapchain, size {}", size);
	else
		DEBUG_AS(cat, "Recreated swapchain, size {}", size);
	return swapchain;
}

GPU::GPU(dev::Window& window):
	// Beautiful, isn't it
	cat{globals::logger->register_category("Graphics", LogLevelGraphics)},
	window{window},
	instance{cat},
	surface{cat, window, instance},
	physical_device{select_physical_device(instance, surface)},
	device{cat, physical_device},
	runtime{vk::create_runtime(instance.instance, device.device, vk::retrieve_device_queues(device.device))},
	global_resource{runtime, FramesInFlight},
	global_allocator{global_resource},
	swapchain{create_swapchain(global_allocator, device, window.size())},
	tracy_context{vk::create_tracy_context(global_allocator)}
{
	INFO_AS(cat, "Vulkan initialized");
}

template<callable<ManagedImage(vk::Allocator&, ManagedImage&&)> Func>
void GPU::frame(Func&& func)
{
	auto frame_allocator = vk::begin_frame(runtime, global_resource);
	auto swapchain_image = vk::acquire_swapchain_image(swapchain, "swp_img");
	auto result = func(frame_allocator, move(swapchain_image));
	vk::submit(frame_allocator, tracy_context, move(result));

}

}
