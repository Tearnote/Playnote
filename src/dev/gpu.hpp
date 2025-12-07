/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
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
	auto create_swapchain(lib::vuk::Allocator& allocator, Device& device, int2 size,
		optional<lib::vuk::Swapchain> old = nullopt) const -> lib::vuk::Swapchain;

	auto estimate_frame_sleep() -> nanoseconds;

	dev::Window& window;

	Instance instance;
	Surface surface;
	lib::vk::PhysicalDevice physical_device;
	Device device;
	lib::vuk::Runtime runtime;
	lib::vuk::GlobalResource global_resource;
	lib::vuk::Allocator global_allocator;
	lib::vuk::Swapchain swapchain;

	nanoseconds last_submit = {};
};

template<callable<ManagedImage(lib::vuk::Allocator&, ManagedImage&&)> Func>
void GPU::frame(Func&& func)
{
	auto const sleep_duration = estimate_frame_sleep();
	if (sleep_duration > 0ns) sleep_for(sleep_duration);

	auto frame_allocator = lib::vuk::begin_frame(runtime, global_resource);
	auto swapchain_image = lib::vuk::acquire_swapchain_image(swapchain, "swp_img");
	auto result = func(frame_allocator, move(swapchain_image));

	auto const before_submit = globals::glfw->get_time();
	lib::vuk::submit(frame_allocator, move(result));
	last_submit = sleep_duration + (globals::glfw->get_time() - before_submit);
}

}
