/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "dev/gpu.hpp"

#include "preamble.hpp"
#include "utils/config.hpp"

namespace playnote::dev {

GPU::Instance::Instance(Logger::Category cat):
	instance{lib::vk::create_instance(AppTitle, cat)},
	cat{cat}
{ DEBUG_AS(cat, "Vulkan instance created"); }


GPU::Instance::~Instance() noexcept
{
	lib::vk::destroy_instance(move(instance));
	DEBUG_AS(cat, "Vulkan instance cleaned up");
}

GPU::Surface::Surface(Logger::Category cat, dev::Window& window, Instance& instance):
	surface{window.create_surface(instance.instance)},
	cat{cat},
	instance{instance}
{}

GPU::Surface::~Surface() noexcept
{
	lib::vk::destroy_surface(instance.instance, surface);
	DEBUG_AS(cat, "Vulkan surface cleaned up");
}

GPU::Device::Device(Logger::Category cat, lib::vk::PhysicalDevice const& physical_device):
	device{lib::vk::create_device(physical_device)},
	cat{cat}
{ DEBUG_AS(cat, "Vulkan device created"); }

GPU::Device::~Device() noexcept
{
	lib::vk::destroy_device(move(device));
	DEBUG_AS(cat, "Vulkan device cleaned up");
}

auto GPU::select_physical_device(Instance const& instance, Surface const& surface) const -> lib::vk::PhysicalDevice
{
	auto physical_device = lib::vk::select_physical_device(instance.instance, surface.surface);
	auto const version = lib::vk::get_driver_version(physical_device);

	INFO_AS(cat, "GPU selected: {}", lib::vk::get_device_name(physical_device));
	DEBUG_AS(cat, "Vulkan driver version {}.{}.{}", version[0], version[1], version[2]);
	return physical_device;
}

auto GPU::create_swapchain(lib::vuk::Allocator& allocator, Device& device, int2 size,
	optional<lib::vuk::Swapchain> old) const -> lib::vuk::Swapchain
{
	auto const recreating = old.has_value();
	auto const requested_images = globals::config->get_entry<int>("graphics", "swapchain_image_count");
	auto swapchain = lib::vuk::create_swapchain(allocator, device.device, size, requested_images, move(old));
	if (!recreating)
		DEBUG_AS(cat, "Created swapchain, size {}", size);
	else
		DEBUG_AS(cat, "Recreated swapchain, size {}", size);
	if (swapchain.images.size() != requested_images)
		WARN_AS(cat, "Requested {} swapchain images, got {} instead", requested_images, swapchain.images.size());
	return swapchain;
}

GPU::GPU(dev::Window& window, Logger::Category cat):
	// Beautiful, isn't it
	cat{cat},
	window{window},
	instance{cat},
	surface{cat, window, instance},
	physical_device{select_physical_device(instance, surface)},
	device{cat, physical_device},
	runtime{lib::vuk::create_runtime(instance.instance, device.device, lib::vk::retrieve_device_queues(device.device))},
	global_resource{runtime, 2},
	global_allocator{global_resource},
	swapchain{create_swapchain(global_allocator, device, window.size())}
{ INFO_AS(cat, "Vulkan initialized"); }

auto GPU::estimate_frame_sleep() -> nanoseconds
{
	if (!globals::config->get_entry<bool>("graphics", "low_latency")) return 0ns;
	auto sleep = last_submit - 2ms; // Leave 2ms for rendergraph and Vulkan overhead
	if (sleep < 2ms) return 0ns; // Don't sleep at all if gain is too small
	if (sleep > 16ms) {
		// Disable sleep if lagspike detected
		WARN_AS(cat, "Renderer lagspike frame: {}ms", last_submit / 1ms);
		return 0ns;
	}
	return sleep;
}

}
