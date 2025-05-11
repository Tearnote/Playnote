module;
#include "tracy/Tracy.hpp"
#include "imgui.h"

export module playnote.render_thread;

import playnote.globals;
import playnote.sys.window;
import playnote.sys.gpu;
import playnote.sys.os;
import playnote.gfx.renderer;

namespace playnote {

// Welcome back, LR2
void enqueue_test_scene(gfx::Renderer::Queue& queue)
{
	queue.enqueue_rect({{ 33, 315}, {256,   6}, {0.996f, 0.000f, 0.000f, 1.000f}});
	queue.enqueue_rect({{ 31,   0}, {  2, 322}, {1.000f, 1.000f, 1.000f, 1.000f}});
	queue.enqueue_rect({{ 31, 321}, {260,   1}, {1.000f, 1.000f, 1.000f, 1.000f}});
	queue.enqueue_rect({{289,   0}, {  2, 322}, {1.000f, 1.000f, 1.000f, 1.000f}});
	queue.enqueue_rect({{ 84,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
	queue.enqueue_rect({{116,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
	queue.enqueue_rect({{141,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
	queue.enqueue_rect({{173,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
	queue.enqueue_rect({{198,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
	queue.enqueue_rect({{230,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
	queue.enqueue_rect({{255,   0}, {  2, 315}, {0.376f, 0.376f, 0.376f, 0.376f}});
}

// Handle the tasks of the render thread, which is to present current game state onto the window
// at the screen's refresh rate.
export void render_thread()
{
	sys::set_thread_name("render");
	auto& window = locator.get<sys::Window>();
	auto& gpu = locator.get<sys::GPU>();
	auto [renderer, renderer_stub] = locator.provide<gfx::Renderer>(gpu);

	while (!window.is_closing()) {
		renderer.frame([](gfx::Renderer::Queue& queue) {
			enqueue_test_scene(queue);
			ImGui::ShowDemoWindow();
		});
		FrameMark;
	}
}

}
