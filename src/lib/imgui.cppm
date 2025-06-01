/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/imgui.cppm:
Imports of Imgui initialization and usage.
*/

module;
#include "backends/imgui_impl_glfw.h"
#include "vuk/extra/ImGuiIntegration.hpp"

export module playnote.lib.imgui;

import playnote.preamble;
import playnote.lib.vulkan;
import playnote.lib.glfw;

namespace playnote::lib::imgui {

// Resources for vuk's Imgui integration.
export using Context = vuk::extra::ImGuiData;

// Initialize Imgui and resources for vuk's Imgui integration.
// Throws if vuk throws.
export auto init(glfw::Window window, vk::Allocator& global_allocator) -> Context
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window, true);
	return vuk::extra::ImGui_ImplVuk_Init(global_allocator);
}

// Mark the start of a new frame for Imgui. All Imgui commands must come after this is called.
export void begin() noexcept
{
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

// Finalize Imgui commands and prepare buffers for rendering. Imgui commands cannot be used anymore.
export void end() noexcept { ImGui::Render(); }

// Draw queued Imgui commands onto the provided image.
// Throws if vuk throws.
export auto render(vk::Allocator& frame_allocator, vk::ManagedImage&& target, Context& context) -> vk::ManagedImage
{
	return vuk::extra::ImGui_ImplVuk_Render(frame_allocator, move(target), context);
}

// A clickable button. Returns true when clicked.
export auto button(char const* text) noexcept -> bool
{
	return ImGui::Button(text);
}

}
