/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/vulkan.cppm:
Imports of Vulkan and helper libraries.
*/

module;
#include <vector> // need std::vector specifically
#include <memory> // need std::unique_ptr specifically
#include "volk.h"
#include "VkBootstrap.h"
#include "vuk/runtime/vk/DeviceFrameResource.hpp"
#include "vuk/runtime/vk/VkSwapchain.hpp"
#include "vuk/runtime/vk/Allocator.hpp"
#include "vuk/runtime/vk/VkRuntime.hpp"
#include "vuk/runtime/vk/Pipeline.hpp"
#include "vuk/runtime/ThisThreadExecutor.hpp"
#include "vuk/runtime/CommandBuffer.hpp" // Required to work around bug in TracyIntegration.hpp
#include "vuk/extra/TracyIntegration.hpp"
#include "vuk/vsl/Core.hpp"
#include "vuk/ImageAttachment.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Value.hpp"
#include "vuk/Types.hpp"
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.lib.vulkan;

import playnote.preamble;
import playnote.config;
import playnote.logger;

namespace playnote::lib::vk {

// A Vulkan instance, representing library context.
export using vkb::Instance;

// Function called whenever a Vulkan logging event occurs. Provides diagnostic info, validation
// messages and shader printf.
auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity_code,
	VkDebugUtilsMessageTypeFlagsEXT type_code, VkDebugUtilsMessengerCallbackDataEXT const* data,
	void* logger_ptr) -> VkBool32
{
	ASSERT(data);
	auto* logger = static_cast<Logger::Category*>(logger_ptr);

	auto const type = [type_code]() {
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

// Create a Vulkan instance.
// Throws runtime_error on failure.
export auto create_instance(string_view name, Logger::Category* debug_logger) -> Instance
{
	auto instance_builder = vkb::InstanceBuilder{}
		.set_app_name(string{name}.c_str())
		.set_engine_name("vuk")
		.require_api_version(1, 2, 0)
		.set_app_version(AppVersion[0], AppVersion[1], AppVersion[2]);
	if constexpr (VulkanValidationEnabled) {
		instance_builder
			.enable_layer("VK_LAYER_KHRONOS_validation")
			.add_validation_feature_enable(
				VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
			.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
			.set_debug_callback(debug_callback)
			.set_debug_callback_user_data_pointer(debug_logger)
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
	auto instance = instance_result.value();

	// Load volk function pointers
	volkInitializeCustom(instance.fp_vkGetInstanceProcAddr);
	volkLoadInstanceOnly(instance);

	return instance;
}

// Destroy the Vulkan instance.
export void destroy_instance(Instance& instance) noexcept { vkb::destroy_instance(instance); }

// A window's surface, as visible for Vulkan as a drawing target.
export using Surface = VkSurfaceKHR;

// Surface creation is handled by lib.window

// Destroy the window's Vulkan surface.
export void destroy_surface(Instance const& instance, Surface surface) noexcept
{
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

// A specific Vulkan-compatible GPU on the system.
export using PhysicalDevice = vkb::PhysicalDevice;

// Choose and return the best available GPU.
// Throws runtime_error if none is found.
export auto select_physical_device(Instance const& instance, Surface surface) -> PhysicalDevice
{
	constexpr auto features = []() {
		auto features = VkPhysicalDeviceFeatures{};
		if constexpr (VulkanValidationEnabled) {
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

	auto physical_device_selector = vkb::PhysicalDeviceSelector{instance}
		.set_surface(surface)
		.set_minimum_version(1, 2) // vuk requirement
		.set_required_features(features)
		.set_required_features_11(vulkan11_features)
		.set_required_features_12(vulkan12_features)
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
	return physical_device;
}

// Return the driver version triple in use by a specific GPU.
export auto get_driver_version(PhysicalDevice const& physical_device) -> array<uint32, 3>
{
	return to_array({
		VK_API_VERSION_MAJOR(physical_device.properties.driverVersion),
		VK_API_VERSION_MINOR(physical_device.properties.driverVersion),
		VK_API_VERSION_PATCH(physical_device.properties.driverVersion)
	});
}

// A logical Vulkan device created from a physical one.
export using Device = vkb::Device;

// Create a Vulkan device out of a physical device.
// Throws runtime_error on failure.
export auto create_device(PhysicalDevice const& physical_device) -> Device
{
	auto device_result = vkb::DeviceBuilder(physical_device).build();
	if (!device_result)
		throw runtime_error_fmt("Failed to create Vulkan device: {}",
			device_result.error().message());
	auto device = vkb::Device(device_result.value());
	volkLoadDevice(device); // Load Vulkan function pointers which weren't provided by the instance
	return device;
}

// Destroy the Vulkan device. Make sure it's idle first.
export void destroy_device(Device& device) noexcept { vkb::destroy_device(device); }

// A queue into which GPU commands can be submitted.
export using Queue = VkQueue;

// Set of queues available on a device. A queue is nullptr if unavailable.
export struct QueueSet {
	Queue graphics;
	uint32 graphics_family_index;
	Queue transfer;
	uint32 transfer_family_index;
	Queue compute;
	uint32 compute_family_index;
};

// Fill in a QueueSet from a Vulkan device.
export auto retrieve_device_queues(Device const& device) -> QueueSet
{
	auto result = QueueSet{};

	result.graphics = device.get_queue(vkb::QueueType::graphics).value();
	result.graphics_family_index = device.get_queue_index(vkb::QueueType::graphics).value();

	auto const transfer_queue_present = device.get_dedicated_queue(vkb::QueueType::transfer).has_value();
	result.transfer = transfer_queue_present?
		device.get_dedicated_queue(vkb::QueueType::transfer).value() :
		VK_NULL_HANDLE;
	result.transfer_family_index = transfer_queue_present?
		device.get_dedicated_queue_index(vkb::QueueType::transfer).value() :
		VK_QUEUE_FAMILY_IGNORED;

	auto const compute_queue_present = device.get_dedicated_queue(vkb::QueueType::compute).has_value();
	result.compute = compute_queue_present?
		device.get_dedicated_queue(vkb::QueueType::compute).value() :
		VK_NULL_HANDLE;
	result.compute_family_index = compute_queue_present?
		device.get_dedicated_queue_index(vkb::QueueType::compute).value() :
		VK_QUEUE_FAMILY_IGNORED;

	return result;
}

// A shorthand for an image attachment currently being managed by vuk's rendergraph.
export using ManagedImage = vuk::Value<vuk::ImageAttachment>;

// A set of Vulkan object references used by vuk.
export using Runtime = vuk::Runtime;

// Initialize vuk by building a Runtime object.
// Throws if vuk throws.
export auto create_runtime(Instance const& instance, Device const& device,
	QueueSet const& queues) -> Runtime
{
	auto const pointers = vuk::FunctionPointers{
		vkGetInstanceProcAddr,
		vkGetDeviceProcAddr,
// Please don't hate me martty, it saves me so much typing
// (and of course we won't spend the extra 1ms it would take to load the functions a second time)
#define VUK_X(name) name,
#define VUK_Y(name) name,
#include "vuk/runtime/vk/VkPFNOptional.hpp"
#include "vuk/runtime/vk/VkPFNRequired.hpp"
	};

	auto executors = std::vector<std::unique_ptr<vuk::Executor>>{};
	executors.reserve(4);
	executors.emplace_back(std::make_unique<vuk::ThisThreadExecutor>());
	executors.emplace_back(vuk::create_vkqueue_executor(pointers, device.device, queues.graphics,
		queues.graphics_family_index, vuk::DomainFlagBits::eGraphicsQueue));
	if (queues.compute) {
		executors.emplace_back(vuk::create_vkqueue_executor(pointers, device.device, queues.compute,
			queues.compute_family_index, vuk::DomainFlagBits::eComputeQueue));
	}
	if (queues.transfer) {
		executors.emplace_back(vuk::create_vkqueue_executor(pointers, device.device,
			queues.transfer, queues.transfer_family_index, vuk::DomainFlagBits::eTransferQueue));
	}

	return Runtime{vuk::RuntimeCreateParameters{
		.instance = instance,
		.device = device.device,
		.physical_device = device.physical_device,
		.executors = move(executors),
		.pointers = pointers,
	}};
}

// An allocator resource providing memory for objects that span multiple frames, and is the parent
// resource for single-frame resources.
export using GlobalResource = vuk::DeviceSuperFrameResource;

// An interface for creating Vulkan objects out of a resource.
export using Allocator = vuk::Allocator;

// An encapsulation of window surface and related access objects.
export using Swapchain = vuk::Swapchain;

// Create a swapchain object from a device's surface. The swapchain image is RGB8 Unorm, non-linear.
// FIFO presentation mode is used.
// Throws runtime_error on failure, or if vuk throws.
export [[nodiscard]] auto create_swapchain(Allocator& allocator, Device& device, uvec2 size,
	optional<Swapchain> old = nullopt) -> Swapchain
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
			.image_view = vuk::ImageView{{}, view},
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
	swapchain.surface = device.surface;
	return swapchain;
}

// Shorthand for vuk's Tracy integration resources.
export using TracyContext = std::unique_ptr<vuk::extra::TracyContext>;

// Create the resources for vuk's Tracy integration.
export [[nodiscard]] auto create_tracy_context(Allocator& allocator) -> TracyContext
{
	return vuk::extra::init_Tracy(allocator);
}

// Start a new frame and create its single-frame allocator.
// Throws if vuk throws.
export auto begin_frame(Runtime& runtime, GlobalResource& resource) -> vk::Allocator
{
	auto& frame_resource = resource.get_next_frame();
	auto frame_allocator = vk::Allocator{frame_resource};
	runtime.next_frame();
	return frame_allocator;
}

// Retrieve and acquire the next image in the swapchain.
// Throws if vuk throws.
export [[nodiscard]] auto acquire_swapchain_image(Swapchain& swapchain,
	string_view name) -> ManagedImage
{
	auto acquired_swapchain = vuk::acquire_swapchain(swapchain);
	return vuk::acquire_next_image(name, move(acquired_swapchain));
}

// Submit the given image for presentation on the swapchain surface.
// Throws if vuk throws.
export void submit(Allocator& allocator, TracyContext const& tracy_context, ManagedImage&& image)
{
	auto entire_thing = vuk::enqueue_presentation(move(image));
	auto compiler = vuk::Compiler{};
	auto const profiling_cbs = vuk::extra::make_Tracy_callbacks(*tracy_context);
	entire_thing.submit(allocator, compiler, {.callbacks = profiling_cbs});
}

// Compile a vertex and fragment shader pair into a graphics pipeline.
// Throws if vuk throws.
export template<usize NVert, usize NFrag>
void create_graphics_pipeline(Runtime& runtime, string_view name,
	array<uint32, NVert> const& vertex_shader, array<uint32, NFrag> const& fragment_shader)
{
	auto const vert_name = format("{}.vert", name);
	auto const frag_name = format("{}.frag", name);
	auto pci = vuk::PipelineBaseCreateInfo{};
	pci.add_static_spirv(vertex_shader.data(), NVert, move(vert_name));
	pci.add_static_spirv(fragment_shader.data(), NFrag, move(frag_name));
	runtime.create_named_pipeline(name, pci);
}

// Clear an image with a solid color.
// Throws if vuk throws.
export auto clear_image(ManagedImage&& input, vec4 color) -> ManagedImage
{
	return vuk::clear_image(move(input), vuk::ClearColor{color.r(), color.g(), color.b(), color.a()});
}

// macros/vuk.hpp support

export using vuk::Access;
export using vuk::Arg;
export using vuk::ImageAttachment;
export using vuk::Buffer;
export using vuk::tag_type;

// Rendergraph support

export using vuk::make_pass;
export using vuk::CommandBuffer;

// Create a host-visible buffer with the provided data. Memory is never freed, so use with
// a frame allocator.
// Throws if vuk throws.
export template<typename T>
auto create_scratch_buffer(Allocator& allocator, span<T> data) -> Buffer
{
	auto [buf, fut] = vuk::create_buffer(allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, data);
	return buf.release();
}

// Set the default command buffer configuration used by this application.
// Throws if vuk throws.
export auto set_cmd_defaults(CommandBuffer& cmd) -> CommandBuffer&
{
	return cmd
		.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
		.set_viewport(0, vuk::Rect2D::framebuffer())
		.set_scissor(0, vuk::Rect2D::framebuffer())
		.set_rasterization({})
		.broadcast_color_blend({});
}

}
