module;
#include <utility>
#include "libassert/assert.hpp"
#include "volk.h"
#include "VkBootstrap.h"
#include "util/log_macros.hpp"
#include "config.hpp"

export module playnote.sys.gpu;

import playnote.stx.except;
import playnote.util.service;
import playnote.util.raii;
import playnote.sys.window;

namespace playnote::sys {

using sys::s_window;

class GPU {
public:
	GPU();

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
	})>;
	using Device = util::RAIIResource<vkb::Device, decltype([](auto& d) {
		vkb::destroy_device(d);
	})>;

#ifdef VK_VALIDATION
	static auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
		VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const*,
		void*) -> VkBool32;
#endif
	auto create_instance() -> Instance;
	auto create_surface(vkb::Instance&) -> Surface;
	auto select_physical_device(vkb::Instance&, VkSurfaceKHR) -> vkb::PhysicalDevice;

	Instance instance{};
	Surface surface{};
	vkb::PhysicalDevice physical_device{};
	Device device{};
};

GPU::GPU():
	instance{create_instance()},
	surface{create_surface(*instance)},
	physical_device{select_physical_device(*instance, *surface)}
//	device{create_device(physical_device)}
{
	L_INFO("Vulkan initialized");
}

#ifdef VK_VALIDATION
auto GPU::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severityCode,
	VkDebugUtilsMessageTypeFlagsEXT typeCode, VkDebugUtilsMessengerCallbackDataEXT const* data,
	void*) -> VkBool32
{
	ASSERT(data);

	auto type = [typeCode]() {
		if (typeCode & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			return "[VulkanPerf]";
		if (typeCode & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
			return "[VulkanSpec]";
		if (typeCode & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
			return "[Vulkan]";
		throw stx::logic_error_fmt("Unknown Vulkan diagnostic message type: #{}", typeCode);
	}();

	if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		L_ERROR("{} {}", type, data->pMessage);
	else if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		L_WARN("{} {}", type, data->pMessage);
	else if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		L_INFO("{} {}", type, data->pMessage);
	else if (severityCode & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		L_DEBUG("{} {}", type, data->pMessage);
	else
		throw stx::logic_error_fmt("Unknown Vulkan diagnostic message severity: #{}",
			std::to_underlying(severityCode));

	return VK_FALSE;
}
#endif

auto GPU::create_instance() -> Instance
{
	auto instance_result = vkb::InstanceBuilder()
#ifdef VK_VALIDATION
		.enable_layer("VK_LAYER_KHRONOS_validation")
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
		.set_debug_callback(debug_callback)
		.set_debug_messenger_severity(
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		)
		.set_debug_messenger_type(
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		)
#endif //VK_VALIDATION
		.set_app_name(AppTitle)
		.set_engine_name("vuk")
		.require_api_version(1, 2, 0)
		.set_app_version(AppVersion[0], AppVersion[1], AppVersion[2])
		.build();
	if (!instance_result)
		throw stx::runtime_error_fmt("Failed to create a Vulkan instance: {}",
			instance_result.error().message());
	auto instance = Instance{vkb::Instance(instance_result.value())};

	volkInitializeCustom(instance->fp_vkGetInstanceProcAddr);
	volkLoadInstanceOnly(instance->instance);

	L_DEBUG("Vulkan instance created");
	return instance;
}

auto GPU::create_surface(vkb::Instance& instance) -> Surface
{
	return Surface_impl{
		.surface = s_window->create_surface(instance),
		.instance = instance,
	};
}

auto GPU::select_physical_device(vkb::Instance& instance,
	VkSurfaceKHR surface) -> vkb::PhysicalDevice
{
	auto physical_device_features = VkPhysicalDeviceFeatures{
#ifdef VK_VALIDATION
		.robustBufferAccess = VK_TRUE,
		.shaderInt64 = VK_TRUE,
#endif //VK_VALIDATION
	};
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

	auto physical_device_selector_result = vkb::PhysicalDeviceSelector(instance)
		.set_surface(surface)
		.set_minimum_version(1, 2)
		.set_required_features(physical_device_features)
		.set_required_features_11(physical_device_vulkan11_features)
		.set_required_features_12(physical_device_vulkan12_features)
		.add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
#ifdef VK_VALIDATION
		.add_required_extension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)
		.add_required_extension_features(VkPhysicalDeviceRobustness2FeaturesEXT{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
			.robustBufferAccess2 = VK_TRUE,
			.robustImageAccess2 = VK_TRUE,
		})
#endif //VK_VALIDATION
		.select();
	if (!physical_device_selector_result) {
		throw stx::runtime_error_fmt("Failed to find a suitable GPU for Vulkan: {}",
			physical_device_selector_result.error().message());
	}
	auto physical_device = physical_device_selector_result.value();
	physical_device.enable_extension_if_present(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
	physical_device.enable_extension_if_present(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	L_INFO("GPU selected: {}", physical_device.properties.deviceName);
	L_DEBUG("Vulkan driver version {}.{}.{}",
		VK_API_VERSION_MAJOR(physical_device.properties.driverVersion),
		VK_API_VERSION_MINOR(physical_device.properties.driverVersion),
		VK_API_VERSION_PATCH(physical_device.properties.driverVersion));
	return physical_device;
}

export auto s_gpu = util::Service<GPU>{};

}
