/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/vulkan.hpp"

#include <memory> // need std::unique_ptr specifically
#include "volk.h"
#include "VkBootstrap.h"
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

namespace playnote::lib::vk {

// Function called whenever a Vulkan logging event occurs. Provides diagnostic info, validation
// messages and shader printf.
static auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity_code,
	VkDebugUtilsMessageTypeFlagsEXT type_code, VkDebugUtilsMessengerCallbackDataEXT const* data,
	void* logger_ptr) -> VkBool32
{
	ASSERT(data);
	auto logger = static_cast<Logger::Category>(logger_ptr);

	auto const type = [type_code] {
		if (type_code & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) return "[VulkanPerf]";
		if (type_code & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) return "[VulkanSpec]";
		if (type_code & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) return "[Vulkan]";
		PANIC("Unknown Vulkan diagnostic message type: #{}", type_code);
	}();

	if (severity_code & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		ERROR_AS(logger, "{} {}", type, data->pMessage);
	else if (severity_code & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		WARN_AS(logger, "{} {}", type, data->pMessage);
	else if (severity_code & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		INFO_AS(logger, "{} {}", type, data->pMessage);
	else if (severity_code & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		DEBUG_AS(logger, "{} {}", type, data->pMessage);
	else
		PANIC("Unknown Vulkan diagnostic message severity: #{}", +severity_code);

	return VK_FALSE;
}

auto create_instance(string_view name, Logger::Category debug_logger) -> Instance
{
	auto instance_builder = vkb::InstanceBuilder{}
		.set_app_name(string{name}.c_str())
		.set_engine_name("vuk")
		.require_api_version(1, 3, 0)
		.set_app_version(AppVersion[0], AppVersion[1], AppVersion[2]);
	if (globals::config->get_entry<bool>("graphics", "validation_enabled")) {
		instance_builder
			.enable_layer("VK_LAYER_KHRONOS_validation")
			.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
			.set_debug_callback(debug_callback)
			.set_debug_callback_user_data_pointer(debug_logger)
			.set_debug_messenger_severity(
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
			)
			.set_debug_messenger_type(
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
			);
	}
	auto instance_result = instance_builder.build();
	if (!instance_result)
		throw runtime_error_fmt("Failed to create a Vulkan instance: {}", instance_result.error().message());
	auto* instance = new vkb::Instance{instance_result.value()};

	// Load volk function pointers
	volkInitializeCustom(instance->fp_vkGetInstanceProcAddr);
	volkLoadInstanceOnly(*instance);

	return instance;
}

void destroy_instance(Instance&& instance) noexcept
{
	vkb::destroy_instance(*instance);
	delete instance;
}

auto get_raw_instance(Instance instance) noexcept -> RawInstance { return instance->instance; }

void destroy_surface(Instance instance, Surface surface) noexcept
{
	vkDestroySurfaceKHR(*instance, surface, nullptr);
}

void detail::PhysicalDeviceDeleter::operator()(vkb::PhysicalDevice* pdev) noexcept
{
	delete pdev;
}

auto select_physical_device(Instance const& instance, Surface surface) -> PhysicalDevice
{
	auto const features = [] {
		auto features = VkPhysicalDeviceFeatures{};
		if (globals::config->get_entry<bool>("graphics", "validation_enabled")) {
			features.robustBufferAccess = VK_TRUE;
			features.shaderInt64 = VK_TRUE;
		}
		return features;
	}();
	// All of the below features are vuk requirements
	constexpr auto vulkan11_features = VkPhysicalDeviceVulkan11Features{
		.shaderDrawParameters = VK_TRUE,
	};
	constexpr auto vulkan12_features = VkPhysicalDeviceVulkan12Features{
		.descriptorIndexing = VK_TRUE,
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE,
		.hostQueryReset = VK_TRUE,
		.timelineSemaphore = VK_TRUE,
		.bufferDeviceAddress = VK_TRUE,
		.vulkanMemoryModel = VK_TRUE,
		.vulkanMemoryModelDeviceScope = VK_TRUE,
		.shaderOutputLayer = VK_TRUE,
	};

	auto physical_device_selector = vkb::PhysicalDeviceSelector{*instance}
		.set_surface(surface)
		.set_minimum_version(1, 3)
		.set_required_features(features)
		.set_required_features_11(vulkan11_features)
		.set_required_features_12(vulkan12_features)
		.add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
		.add_required_extension_features(VkPhysicalDeviceSynchronization2FeaturesKHR{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
			.synchronization2 = VK_TRUE,
		});
	if (globals::config->get_entry<bool>("graphics", "validation_enabled")) {
		physical_device_selector
			.add_required_extension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)
			.add_required_extension_features(VkPhysicalDeviceRobustness2FeaturesEXT{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
				.robustBufferAccess2 = VK_TRUE,
				.robustImageAccess2 = VK_TRUE,
			});
	}
	auto physical_device_selector_result = physical_device_selector.select();
	if (!physical_device_selector_result) {
		throw runtime_error_fmt("Failed to find a suitable GPU for Vulkan: {}",
			physical_device_selector_result.error().message());
	}
	auto physical_device = PhysicalDevice{new vkb::PhysicalDevice{physical_device_selector_result.value()}};
	physical_device->enable_extension_if_present(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
	return physical_device;
}

auto get_driver_version(PhysicalDevice const& physical_device) -> array<uint, 3>
{
	return to_array({
		VK_API_VERSION_MAJOR(physical_device->properties.driverVersion),
		VK_API_VERSION_MINOR(physical_device->properties.driverVersion),
		VK_API_VERSION_PATCH(physical_device->properties.driverVersion)
	});
}

auto get_device_name(PhysicalDevice const& physical_device) -> string_view
{
	return physical_device->properties.deviceName;
}

auto create_device(PhysicalDevice const& physical_device) -> Device
{
	auto device_result = vkb::DeviceBuilder(*physical_device).build();
	if (!device_result)
		throw runtime_error_fmt("Failed to create Vulkan device: {}",
			device_result.error().message());
	auto device = new vkb::Device{device_result.value()};
	volkLoadDevice(*device); // Load Vulkan function pointers which weren't provided by the instance
	return device;
}

void destroy_device(Device&& device) noexcept
{
	vkb::destroy_device(*device);
	delete device;
}

auto retrieve_device_queues(Device device) -> QueueSet
{
	auto result = QueueSet{};

	result.graphics = device->get_queue(vkb::QueueType::graphics).value();
	result.graphics_family_index = device->get_queue_index(vkb::QueueType::graphics).value();

	auto const transfer_queue_present = device->get_dedicated_queue(vkb::QueueType::transfer).has_value();
	result.transfer = transfer_queue_present?
		device->get_dedicated_queue(vkb::QueueType::transfer).value() :
		VK_NULL_HANDLE;
	result.transfer_family_index = transfer_queue_present?
		device->get_dedicated_queue_index(vkb::QueueType::transfer).value() :
		VK_QUEUE_FAMILY_IGNORED;

	auto const compute_queue_present = device->get_dedicated_queue(vkb::QueueType::compute).has_value();
	result.compute = compute_queue_present?
		device->get_dedicated_queue(vkb::QueueType::compute).value() :
		VK_NULL_HANDLE;
	result.compute_family_index = compute_queue_present?
		device->get_dedicated_queue_index(vkb::QueueType::compute).value() :
		VK_QUEUE_FAMILY_IGNORED;

	return result;
}

}
