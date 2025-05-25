/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.cppm:
Presents current game state onto the window at the screen's refresh rate.
*/

module;
#include "imgui.h"
#include "macros/tracing.hpp"
#include "macros/logger.hpp"

export module playnote.threads.render;

import playnote.preamble;
import playnote.logger;
import playnote.sys.window;
import playnote.sys.gpu;
import playnote.sys.os;
import playnote.gfx.renderer;

namespace playnote::threads {

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

export void render(sys::Window& window)
try {
	sys::set_thread_name("render");
	auto gpu = sys::GPU{window};
	auto renderer = gfx::Renderer{gpu};

	while (!window.is_closing()) {
		renderer.frame([](gfx::Renderer::Queue& queue) {
			enqueue_test_scene(queue);
			ImGui::ShowDemoWindow();
		});
		FRAME_MARK();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
