module;
#include "vuk/extra/ImGuiIntegration.hpp"
#include "backends/imgui_impl_glfw.h"

export module playnote.gfx.imgui;

import playnote.stx.callable;
import playnote.sys.window;
import playnote.sys.gpu;

namespace playnote::gfx {

export class Imgui {
public:
	Imgui(sys::GPU&);

	template<stx::callable<void()> Func>
	void enqueue(Func);

	auto draw(vuk::Allocator&, sys::ManagedImage) -> sys::ManagedImage;

private:
	vuk::extra::ImGuiData imgui_data;
};

Imgui::Imgui(sys::GPU& gpu)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(gpu.get_window().handle(), true);
	imgui_data = vuk::extra::ImGui_ImplVuk_Init(gpu.get_global_allocator());
}

template<stx::callable<void()> Func>
void Imgui::enqueue(Func func)
{
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	func();
	ImGui::Render();
}

auto Imgui::draw(vuk::Allocator& allocator, sys::ManagedImage target) -> sys::ManagedImage
{
	return vuk::extra::ImGui_ImplVuk_Render(allocator, std::move(target), imgui_data);
}

}
