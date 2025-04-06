module;
#include <optional>
#include <utility>
#include <memory>
#include <vector>
#include <ranges>
#include <span>
#include "libassert/assert.hpp"
#include "volk.h"
#include "VkBootstrap.h"
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/vk/VkRuntime.hpp"
#include "vuk/runtime/vk/Image.hpp"
#include "vuk/runtime/ThisThreadExecutor.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Executor.hpp"
#include "vuk/Types.hpp"
#include "vuk/Value.hpp"
#include "util/log_macros.hpp"
#include "config.hpp"

export module playnote.sys.gpu;

import playnote.stx.callable;
import playnote.stx.except;
import playnote.stx.types;
import playnote.stx.math;
import playnote.util.service;
import playnote.util.raii;
import playnote.sys.window;

namespace playnote::sys {

namespace ranges = std::ranges;
using stx::uint;
using stx::uvec2;
using sys::s_window;

export using ManagedImage = vuk::Value<vuk::ImageAttachment>;

class GPU {
public:
	static constexpr auto FramesInFlight = 2u;

	GPU();
	~GPU() { runtime.wait_idle(); }

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
	static auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
		VkDebugUtilsMessageTypeFlagsEXT, VkDebugUtilsMessengerCallbackDataEXT const*,
		void*) -> VkBool32;
#endif
	static auto create_instance() -> Instance;
	static auto create_surface(vkb::Instance&) -> Surface;
	static auto select_physical_device(vkb::Instance&, VkSurfaceKHR) -> vkb::PhysicalDevice;
	static auto create_device(vkb::PhysicalDevice&) -> Device;
	static auto retrieve_queues(vkb::Device&) -> Queues;
	static auto create_runtime(VkInstance, VkPhysicalDevice, VkDevice, Queues const&) -> vuk::Runtime;
	static auto create_swapchain(uvec2 size, vuk::Allocator&, vkb::Device&, Surface&, std::optional<vuk::Swapchain> old = std::nullopt) -> vuk::Swapchain;

	Instance instance{};
	Surface surface{};
	vkb::PhysicalDevice physical_device{};
	Device device{};
	vuk::Runtime runtime;
	vuk::DeviceSuperFrameResource global_resource;
	vuk::Allocator global_allocator;
	vuk::Swapchain swapchain;
};

GPU::GPU():
	instance{create_instance()},
	surface{create_surface(*instance)},
	physical_device{select_physical_device(*instance, *surface)},
	device{create_device(physical_device)},
	runtime{create_runtime(*instance, physical_device, *device, retrieve_queues(*device))},
	global_resource{vuk::DeviceSuperFrameResource{runtime, FramesInFlight}},
	global_allocator{vuk::Allocator{global_resource}},
	swapchain{create_swapchain(s_window->size(), global_allocator, *device, surface)}
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
	entire_thing.submit(frame_allocator, compiler, {});
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

	auto physical_device_selector_result = vkb::PhysicalDeviceSelector{instance}
		.set_surface(surface)
		.set_minimum_version(1, 2)
		.set_required_features(physical_device_features)
		.set_required_features_11(physical_device_vulkan11_features)
		.set_required_features_12(physical_device_vulkan12_features)
		.add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
		.add_required_extension_features(VkPhysicalDeviceSynchronization2FeaturesKHR{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
			.synchronization2 = VK_TRUE,
		})
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

auto GPU::create_device(vkb::PhysicalDevice& physical_device) -> Device
{
	auto device_result = vkb::DeviceBuilder(physical_device).build();
	if (!device_result)
		throw stx::runtime_error_fmt("Failed to create Vulkan device: {}",
			device_result.error().message());
	auto device = Device{vkb::Device(device_result.value())};
	volkLoadDevice(*device);

	L_DEBUG("Vulkan device created");
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
#define VUK_X(name) name,
#define VUK_Y(name) name,
#include "vuk/runtime/vk/VkPFNOptional.hpp"
#include "vuk/runtime/vk/VkPFNRequired.hpp"
	};

	auto executors = std::vector<std::unique_ptr<vuk::Executor>>{};
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
		.executors = std::move(executors),
		.pointers = pointers,
	}};
}

auto GPU::create_swapchain(uvec2 size, vuk::Allocator& allocator, vkb::Device& device, Surface& surface, std::optional<vuk::Swapchain> old) -> vuk::Swapchain
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
		throw stx::runtime_error_fmt("Failed to create the swapchain: {}", vkbswapchain_result.error().message());
	auto vkbswapchain = vkbswapchain_result.value();

	if (old) {
		allocator.deallocate(std::span{&old->swapchain, 1});
		for (auto& image: old->images)
			allocator.deallocate(std::span{&image.image_view, 1});
	}

	auto swapchain = vuk::Swapchain{allocator, vkbswapchain.image_count};

	for(auto [image, view]: ranges::zip_view{*vkbswapchain.get_images(), *vkbswapchain.get_image_views()}) {
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
	swapchain.surface = *surface;
	L_DEBUG("Swapchain (re)created at {}x{}", size.x(), size.y());

	return std::move(swapchain);
}

export auto s_gpu = util::Service<GPU>{};

}
