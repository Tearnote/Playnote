/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/logger.hpp"

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
auto create_instance(string_view name, Logger::Category debug_logger) -> Instance;

// Destroy the Vulkan instance.
void destroy_instance(Instance&& instance) noexcept;

// Retrieve the raw instance handle.
auto get_raw_instance(Instance instance) noexcept -> RawInstance;

// A window's surface, as visible for Vulkan as a drawing target.
using Surface = VkSurfaceKHR_T*;

// Surface creation is handled by lib/glfw

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
auto get_driver_version(PhysicalDevice const& physical_device) -> array<uint, 3>;

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
	int graphics_family_index;
	Queue transfer;
	int transfer_family_index;
	Queue compute;
	int compute_family_index;
};

// Fill in a QueueSet from a Vulkan device.
auto retrieve_device_queues(Device device) -> QueueSet;

}
