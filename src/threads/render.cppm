/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.cppm:
Presents current game state onto the window at the screen's refresh rate.
*/

module;
#include "macros/tracing.hpp"
#include "macros/logger.hpp"

export module playnote.threads.render;

import playnote.preamble;
import playnote.logger;
import playnote.dev.window;
import playnote.dev.gpu;
import playnote.dev.os;
import playnote.gfx.renderer;

namespace playnote::threads {

void enqueue_test_scene(gfx::Renderer::Queue& queue)
{
	queue.enqueue_rect({{ 44, 539}, {342,   6}, {1.000f, 0.200f, 0.200f, 1.000f}});
	queue.enqueue_rect({{ 42,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect({{ 42, 545}, {346,   2}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect({{386,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect({{116,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{158,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{192,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{234,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{268,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{310,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{344,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{118,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect({{194,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect({{270,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect({{346,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});

	queue.enqueue_rect({{478, 539}, {342,   6}, {1.000f, 0.200f, 0.200f, 1.000f}});
	queue.enqueue_rect({{476,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect({{476, 545}, {346,   2}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect({{820,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect({{518,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{552,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{594,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{628,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{670,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{704,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{746,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect({{478,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect({{554,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect({{630,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect({{706,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
}

export void render(dev::Window& window)
try {
	dev::name_current_thread("render");
	auto gpu = dev::GPU{window};
	auto renderer = gfx::Renderer{gpu};

	while (!window.is_closing()) {
		renderer.frame([](gfx::Renderer::Queue& queue) {
			enqueue_test_scene(queue);
		});
		FRAME_MARK();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
