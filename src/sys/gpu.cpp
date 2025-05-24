/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

sys/gpu.cpp:
sys/gpu implementation unit, to keep some of the larger imports out of the interface.
*/

module;
#include <vector>
#include "libassert/assert.hpp"
#include "volk.h"
#include "VkBootstrap.h"
#include "vuk/runtime/vk/VkSwapchain.hpp"
#include "vuk/runtime/vk/VkRuntime.hpp"
#include "vuk/runtime/vk/Allocator.hpp"
#include "vuk/runtime/ThisThreadExecutor.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/Types.hpp"
#include "util/logger.hpp"

module playnote.sys.gpu;

import playnote.preamble;
import playnote.config;

namespace playnote::sys {

auto GPU::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severityCode,
	VkDebugUtilsMessageTypeFlagsEXT typeCode, VkDebugUtilsMessengerCallbackDataEXT const* data,
	void* cat_ptr) -> VkBool32
{
	ASSERT(data);
	auto* cat = static_cast<util::Logger::Category*>(cat_ptr);

	auto type = [typeCode]() {
		if (typeCode & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			return "[VulkanPerf]";
		if (typeCode & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
			return "[VulkanSpec]";
		if (typeCode & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			return "[Vulkan]";
		throw logic_error_fmt("Unknown Vulkan diagnostic message type: #{}", typeCode);
	}();

	if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		ERROR_AS(cat, "{} {}", type, data->pMessage);
	else if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		WARN_AS(cat, "{} {}", type, data->pMessage);
	else if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		INFO_AS(cat, "{} {}", type, data->pMessage);
	else if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		DEBUG_AS(cat, "{} {}", type, data->pMessage);
	else
		throw logic_error_fmt("Unknown Vulkan diagnostic message severity: #{}",
			to_underlying(severityCode));

	return VK_FALSE;
}

auto GPU::create_instance() -> vkb::Instance
{
	auto instance_builder = vkb::InstanceBuilder{}
		.set_app_name(AppTitle)
		.set_engine_name("vuk")
		.require_api_version(1, 2, 0)
		.set_app_version(AppVersion[0], AppVersion[1], AppVersion[2]);
	if constexpr (VulkanValidationEnabled) {
		instance_builder
			.enable_layer("VK_LAYER_KHRONOS_validation")
			.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
			.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
			.set_debug_callback(debug_callback)
			.set_debug_callback_user_data_pointer(cat)
			.set_debug_messenger_severity(
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT // Verbose, but debug printf messages are INFO
			)
			.set_debug_messenger_type(
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
			);
	}
	auto instance_result = instance_builder.build();
	if (!instance_result)
		throw runtime_error_fmt("Failed to create a Vulkan instance: {}",
			instance_result.error().message());
	auto instance = vkb::Instance{instance_result.value()};

	volkInitializeCustom(instance.fp_vkGetInstanceProcAddr);
	volkLoadInstanceOnly(instance.instance);

	DEBUG_AS(cat, "Vulkan instance created");
	return instance;
}

auto GPU::create_surface(vkb::Instance& instance) -> Surface_impl
{
	return Surface_impl{
		.surface = window.create_surface(instance),
		.instance = instance,
	};
}

auto GPU::select_physical_device(vkb::Instance& instance,
	VkSurfaceKHR surface) -> vkb::PhysicalDevice
{
	auto physical_device_features = VkPhysicalDeviceFeatures{};
	if constexpr (VulkanValidationEnabled) {
		physical_device_features.robustBufferAccess = VK_TRUE;
		physical_device_features.shaderInt64 = VK_TRUE;
	}
	// All of the below features are vuk requirements
	auto physical_device_vulkan11_features = VkPhysicalDeviceVulkan11Features{
		.shaderDrawParameters = VK_TRUE,
	};
	auto physical_device_vulkan12_features = VkPhysicalDeviceVulkan12Features{
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

	auto physical_device_selector = vkb::PhysicalDeviceSelector{instance}
		.set_surface(surface)
		.set_minimum_version(1, 2) // vuk requirement
		.set_required_features(physical_device_features)
		.set_required_features_11(physical_device_vulkan11_features)
		.set_required_features_12(physical_device_vulkan12_features)
		.add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
		.add_required_extension_features(VkPhysicalDeviceSynchronization2FeaturesKHR{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
			.synchronization2 = VK_TRUE,
		});
	if (VulkanValidationEnabled) {
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
	auto physical_device = physical_device_selector_result.value();
	physical_device.enable_extension_if_present(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME); // for vuk's Tracy integration
	physical_device.enable_extension_if_present(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	INFO_AS(cat, "GPU selected: {}", physical_device.properties.deviceName);
	DEBUG_AS(cat, "Vulkan driver version {}.{}.{}",
		VK_API_VERSION_MAJOR(physical_device.properties.driverVersion),
		VK_API_VERSION_MINOR(physical_device.properties.driverVersion),
		VK_API_VERSION_PATCH(physical_device.properties.driverVersion));
	return physical_device;
}

auto GPU::create_device(vkb::PhysicalDevice& physical_device) -> vkb::Device
{
	auto device_result = vkb::DeviceBuilder(physical_device).build();
	if (!device_result)
		throw runtime_error_fmt("Failed to create Vulkan device: {}",
			device_result.error().message());
	auto device = vkb::Device(device_result.value());
	volkLoadDevice(device);

	DEBUG_AS(cat, "Vulkan device created");
	return device;
}

auto GPU::retrieve_queues(vkb::Device& device) -> Queues
{
	auto result = Queues{};

	result.graphics = device.get_queue(vkb::QueueType::graphics).value();
	result.graphics_family_index = device.get_queue_index(vkb::QueueType::graphics).value();

	auto transfer_queue_present = device.get_dedicated_queue(vkb::QueueType::transfer).has_value();
	result.transfer = transfer_queue_present?
		device.get_dedicated_queue(vkb::QueueType::transfer).value() :
		VK_NULL_HANDLE;
	result.transfer_family_index = transfer_queue_present?
		device.get_dedicated_queue_index(vkb::QueueType::transfer).value() :
		VK_QUEUE_FAMILY_IGNORED;

	auto compute_queue_present = device.get_dedicated_queue(vkb::QueueType::compute).has_value();
	result.compute = compute_queue_present?
		device.get_dedicated_queue(vkb::QueueType::compute).value() :
		VK_NULL_HANDLE;
	result.compute_family_index = compute_queue_present?
		device.get_dedicated_queue_index(vkb::QueueType::compute).value() :
		VK_QUEUE_FAMILY_IGNORED;

	return result;
}

auto GPU::create_runtime(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device,
	Queues const& queues) -> vuk::Runtime
{
	auto pointers = vuk::FunctionPointers{
		vkGetInstanceProcAddr,
		vkGetDeviceProcAddr,
// Please don't hate me martty, it saves me so much typing
// (and of course we won't spend the extra 1ms it would take to load the functions a second time)
#define VUK_X(name) name,
#define VUK_Y(name) name,
#include "vuk/runtime/vk/VkPFNOptional.hpp"
#include "vuk/runtime/vk/VkPFNRequired.hpp"
	};

	auto executors = std::vector<unique_ptr<vuk::Executor>>{};
	executors.reserve(4);
	executors.emplace_back(std::make_unique<vuk::ThisThreadExecutor>());
	executors.emplace_back(vuk::create_vkqueue_executor(pointers, device, queues.graphics,
		queues.graphics_family_index, vuk::DomainFlagBits::eGraphicsQueue));
	if (queues.compute) {
		executors.emplace_back(vuk::create_vkqueue_executor(pointers, device, queues.compute,
			queues.compute_family_index, vuk::DomainFlagBits::eComputeQueue));
	}
	if (queues.transfer) {
		executors.emplace_back(vuk::create_vkqueue_executor(pointers, device, queues.transfer,
			queues.transfer_family_index, vuk::DomainFlagBits::eTransferQueue));
	}

	return vuk::Runtime{vuk::RuntimeCreateParameters{
		.instance = instance,
		.device = device,
		.physical_device = physical_device,
		.executors = move(executors),
		.pointers = pointers,
	}};
}

auto GPU::create_swapchain(uvec2 size, vuk::Allocator& allocator, vkb::Device& device,
	Surface_impl& surface, optional<vuk::Swapchain> old) -> vuk::Swapchain
{
	auto vkbswapchain_result = vkb::SwapchainBuilder{device}
		.set_old_swapchain(old? old->swapchain : VK_NULL_HANDLE)
		.set_desired_extent(size.x(), size.y())
		.set_desired_format({
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		})
		.add_fallback_format({
			.format = VK_FORMAT_B8G8R8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		})
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_image_usage_flags(
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		)
		.build();
	if (!vkbswapchain_result)
		throw runtime_error_fmt("Failed to create the swapchain: {}",
			vkbswapchain_result.error().message());
	auto vkbswapchain = vkbswapchain_result.value();

	if (old) {
		allocator.deallocate(span{&old->swapchain, 1});
		for (auto& image: old->images)
			allocator.deallocate(span{&image.image_view, 1});
	}

	auto swapchain = vuk::Swapchain{allocator, vkbswapchain.image_count};

	for (auto [image, view]: views::zip(*vkbswapchain.get_images(),
		     *vkbswapchain.get_image_views())) {
		swapchain.images.emplace_back(vuk::ImageAttachment{
			.image = vuk::Image{image, nullptr},
			.image_view = vuk::ImageView{{0}, view},
			.extent = {vkbswapchain.extent.width, vkbswapchain.extent.height, 1},
			.format = static_cast<vuk::Format>(vkbswapchain.image_format),
			.sample_count = vuk::Samples::e1,
			.view_type = vuk::ImageViewType::e2D,
			.base_level = 0,
			.level_count = 1,
			.base_layer = 0,
			.layer_count = 1,
		});
	}

	swapchain.swapchain = vkbswapchain.swapchain;
	swapchain.surface = surface;
	DEBUG_AS(cat, "Swapchain (re)created at {}x{}", size.x(), size.y());

	return move(swapchain);
}

}
