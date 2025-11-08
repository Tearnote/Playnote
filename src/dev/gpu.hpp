/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "lib/vulkan.hpp"
#include "lib/vuk.hpp"
#include "dev/window.hpp"

namespace playnote::dev {

using lib::vuk::ManagedImage;

// RAII encapsulation of GPU state, handling initialization and frame preparation/presentation
class GPU {
public:
	GPU(dev::Window&, Logger::Category);
	~GPU() { runtime.wait_idle(); }

	[[nodiscard]] auto get_window() const -> dev::Window& { return window; }
	[[nodiscard]] auto get_global_allocator() -> lib::vuk::Allocator& { return global_allocator; }

	// Prepare and present a single frame. All vuk draw commands must be submitted within
	// the callback. The callback is provided with the frame allocator and swapchain image.
	template<callable<ManagedImage(lib::vuk::Allocator&, ManagedImage&&)> Func>
	void frame(Func&&);

	GPU(GPU const&) = delete;
	auto operator=(GPU const&) -> GPU& = delete;
	GPU(GPU&&) = delete;
	auto operator=(GPU&&) -> GPU& = delete;

private:
	InstanceLimit<GPU, 1> instance_limit;
	Logger::Category cat;

	// RAII wrapper of a Vulkan instance.
	class Instance {
	public:
		lib::vk::Instance instance;

		explicit Instance(Logger::Category);
		~Instance() noexcept;

		Instance(Instance const&) = delete;
		auto operator=(Instance const&) -> Instance& = delete;
		Instance(Instance&&) = delete;
		auto operator=(Instance&&) -> Instance& = delete;

	private:
		Logger::Category cat;
	};

	// RAII wrapper of a Vulkan surface.
	class Surface {
	public:
		lib::vk::Surface surface;

		Surface(Logger::Category, dev::Window&, Instance&);
		~Surface() noexcept;

		Surface(Surface const&) = delete;
		auto operator=(Surface const&) -> Surface& = delete;
		Surface(Surface&&) = delete;
		auto operator=(Surface&&) -> Surface& = delete;

	private:
		Logger::Category cat;
		Instance& instance;
	};

	// RAII wrapper of a Vulkan device.
	class Device {
	public:
		lib::vk::Device device;

		Device(Logger::Category, lib::vk::PhysicalDevice const&);
		~Device() noexcept;

		Device(Device const&) = delete;
		auto operator=(Device const&) -> Device& = delete;
		Device(Device&&) = delete;
		auto operator=(Device&&) -> Device& = delete;

	private:
		Logger::Category cat;
	};

	// Logging wrappers.
	[[nodiscard]] auto select_physical_device(Instance const&, Surface const&) const -> lib::vk::PhysicalDevice;
	auto create_swapchain(lib::vuk::Allocator& allocator, Device& device, uint2 size,
		optional<lib::vuk::Swapchain> old = nullopt) const -> lib::vuk::Swapchain;

	dev::Window& window;

	Instance instance;
	Surface surface;
	lib::vk::PhysicalDevice physical_device;
	Device device;
	lib::vuk::Runtime runtime;
	lib::vuk::GlobalResource global_resource;
	lib::vuk::Allocator global_allocator;
	lib::vuk::Swapchain swapchain;
};

inline GPU::Instance::Instance(Logger::Category cat):
	instance{lib::vk::create_instance(AppTitle, cat)},
	cat{cat}
{ DEBUG_AS(cat, "Vulkan instance created"); }

inline GPU::Instance::~Instance() noexcept
{
	lib::vk::destroy_instance(move(instance));
	DEBUG_AS(cat, "Vulkan instance cleaned up");
}

inline GPU::Surface::Surface(Logger::Category cat, dev::Window& window, Instance& instance):
	surface{window.create_surface(instance.instance)},
	cat{cat},
	instance{instance} {}

inline GPU::Surface::~Surface() noexcept
{
	lib::vk::destroy_surface(instance.instance, surface);
	DEBUG_AS(cat, "Vulkan surface cleaned up");
}

inline GPU::Device::Device(Logger::Category cat, lib::vk::PhysicalDevice const& physical_device):
	device{lib::vk::create_device(physical_device)},
	cat{cat}
{ DEBUG_AS(cat, "Vulkan device created"); }

inline GPU::Device::~Device() noexcept
{
	lib::vk::destroy_device(move(device));
	DEBUG_AS(cat, "Vulkan device cleaned up");
}

[[nodiscard]] inline auto GPU::select_physical_device(Instance const& instance, Surface const& surface) const -> lib::vk::PhysicalDevice
{
	auto physical_device = lib::vk::select_physical_device(instance.instance, surface.surface);
	auto const version = lib::vk::get_driver_version(physical_device);

	INFO_AS(cat, "GPU selected: {}", lib::vk::get_device_name(physical_device));
	DEBUG_AS(cat, "Vulkan driver version {}.{}.{}", version[0], version[1], version[2]);
	return physical_device;
}

auto inline GPU::create_swapchain(lib::vuk::Allocator& allocator, Device& device, uint2 size,
	optional<lib::vuk::Swapchain> old) const -> lib::vuk::Swapchain
{
	auto const recreating = old.has_value();
	auto swapchain = lib::vuk::create_swapchain(allocator, device.device, size, move(old));
	if (!recreating)
		DEBUG_AS(cat, "Created swapchain, size {}", size);
	else
		DEBUG_AS(cat, "Recreated swapchain, size {}", size);
	return swapchain;
}

inline GPU::GPU(dev::Window& window, Logger::Category cat):
	// Beautiful, isn't it
	cat{cat},
	window{window},
	instance{cat},
	surface{cat, window, instance},
	physical_device{select_physical_device(instance, surface)},
	device{cat, physical_device},
	runtime{lib::vuk::create_runtime(instance.instance, device.device, lib::vk::retrieve_device_queues(device.device))},
	global_resource{runtime, static_cast<uint32>(globals::config->get_entry<int32>("vulkan", "frames_in_flight"))},
	global_allocator{global_resource},
	swapchain{create_swapchain(global_allocator, device, window.size())}
{
	INFO_AS(cat, "Vulkan initialized");
}

template<callable<ManagedImage(lib::vuk::Allocator&, ManagedImage&&)> Func>
void GPU::frame(Func&& func)
{
	auto frame_allocator = lib::vuk::begin_frame(runtime, global_resource);
	auto swapchain_image = lib::vuk::acquire_swapchain_image(swapchain, "swp_img");
	auto result = func(frame_allocator, move(swapchain_image));
	lib::vuk::submit(frame_allocator, move(result));

}

}
