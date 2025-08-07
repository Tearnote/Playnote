/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/imgui.hpp:
Wrapper for vuk's Imgui integration.
*/

#pragma once
#include "preamble.hpp"
#include "lib/vulkan.hpp"
#include "lib/imgui.hpp"
#include "dev/gpu.hpp"

namespace playnote::gfx {

// Encapsulation of Dear ImGui initialization and drawing.
class Imgui {
public:
	explicit Imgui(dev::GPU& gpu):
		context{lib::imgui::init(gpu.get_window().handle(), gpu.get_global_allocator())}
	{}

	// Prepare Imgui to accept commands.
	// All imgui:: functions must be run within the provided function.
	template<callable<void()> Func>
	void enqueue(Func);

	// Draw enqueued Imgui state into the image. Must be run once and after enqueue().
	auto draw(lib::vk::Allocator&, dev::ManagedImage) -> dev::ManagedImage;

private:
	InstanceLimit<Imgui, 1> instance_limit;
	lib::imgui::Context context;
};

template<callable<void()> Func>
void Imgui::enqueue(Func func)
{
	lib::imgui::begin();
	func();
	lib::imgui::end();
}

inline auto Imgui::draw(lib::vk::Allocator& allocator, dev::ManagedImage target) -> dev::ManagedImage
{
	return lib::imgui::render(allocator, move(target), context);
}

}
