/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/imgui.cppm:
Wrapper for vuk's Imgui integration.
*/

export module playnote.gfx.imgui;

import playnote.preamble;
import playnote.lib.vulkan;
import playnote.lib.imgui;
import playnote.dev.gpu;

namespace playnote::gfx {

namespace imgui = lib::imgui;
namespace vk = lib::vk;

// Encapsulation of Dear ImGui initialization and drawing.
export class Imgui {
public:
	explicit Imgui(dev::GPU& gpu):
		context{imgui::init(gpu.get_window().handle(), gpu.get_global_allocator())}
	{}

	// Prepare Imgui to accept commands.
	// All imgui:: functions must be run within the provided function.
	template<callable<void()> Func>
	void enqueue(Func);

	// Draw enqueued Imgui state into the image. Must be run once and after enqueue().
	auto draw(vk::Allocator&, dev::ManagedImage) -> dev::ManagedImage;

private:
	InstanceLimit<Imgui, 1> instance_limit;
	imgui::Context context;
};

template<callable<void()> Func>
void Imgui::enqueue(Func func)
{
	imgui::begin();
	func();
	imgui::end();
}

auto Imgui::draw(vk::Allocator& allocator, dev::ManagedImage target) -> dev::ManagedImage
{
	return imgui::render(allocator, move(target), context);
}

}
