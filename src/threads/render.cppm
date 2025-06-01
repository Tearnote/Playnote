/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.cppm:
Presents current game state onto the window at the screen's refresh rate.
*/

module;
#include "macros/tracing.hpp"
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.threads.render;

import playnote.preamble;
import playnote.logger;
import playnote.dev.window;
import playnote.dev.gpu;
import playnote.dev.os;
import playnote.gfx.renderer;
import playnote.bms.chart;
import playnote.threads.broadcaster;

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

void enqueue_chart_objects(gfx::Renderer::Queue& queue, bms::Chart const& chart)
{
	enum class NoteVisual { White, Blue, Red };
	auto get_note_visual = [](bms::Chart::NoteType type) {
		switch (type) {
		case bms::Chart::NoteType::P1_KeyS:
		case bms::Chart::NoteType::P2_KeyS:
			return NoteVisual::Red;
		case bms::Chart::NoteType::P1_Key1:
		case bms::Chart::NoteType::P1_Key3:
		case bms::Chart::NoteType::P1_Key5:
		case bms::Chart::NoteType::P1_Key7:
		case bms::Chart::NoteType::P2_Key1:
		case bms::Chart::NoteType::P2_Key3:
		case bms::Chart::NoteType::P2_Key5:
		case bms::Chart::NoteType::P2_Key7:
			return NoteVisual::White;
		case bms::Chart::NoteType::P1_Key2:
		case bms::Chart::NoteType::P1_Key4:
		case bms::Chart::NoteType::P1_Key6:
		case bms::Chart::NoteType::P2_Key2:
		case bms::Chart::NoteType::P2_Key4:
		case bms::Chart::NoteType::P2_Key6:
			return NoteVisual::Blue;
		default: PANIC();
		}
	};
	auto get_note_x = [](bms::Chart::NoteType type) {
		switch (type) {
		case bms::Chart::NoteType::P1_KeyS: return 44;
		case bms::Chart::NoteType::P1_Key1: return 118;
		case bms::Chart::NoteType::P1_Key2: return 160;
		case bms::Chart::NoteType::P1_Key3: return 194;
		case bms::Chart::NoteType::P1_Key4: return 236;
		case bms::Chart::NoteType::P1_Key5: return 270;
		case bms::Chart::NoteType::P1_Key6: return 312;
		case bms::Chart::NoteType::P1_Key7: return 346;
		case bms::Chart::NoteType::P2_Key1: return 478;
		case bms::Chart::NoteType::P2_Key2: return 520;
		case bms::Chart::NoteType::P2_Key3: return 554;
		case bms::Chart::NoteType::P2_Key4: return 596;
		case bms::Chart::NoteType::P2_Key5: return 630;
		case bms::Chart::NoteType::P2_Key6: return 672;
		case bms::Chart::NoteType::P2_Key7: return 706;
		case bms::Chart::NoteType::P2_KeyS: return 748;
		default: PANIC();
		}
	};
	auto get_note_color = [](NoteVisual visual) {
		switch (visual) {
		case NoteVisual::White: return vec4{0.800f, 0.800f, 0.800f, 1.000f};
		case NoteVisual::Blue:  return vec4{0.200f, 0.600f, 0.800f, 1.000f};
		case NoteVisual::Red:   return vec4{0.800f, 0.200f, 0.200f, 1.000f};
		default: PANIC();
		}
	};
	auto get_note_width = [](NoteVisual visual) {
		switch (visual) {
		case NoteVisual::White: return 40;
		case NoteVisual::Blue:  return 32;
		case NoteVisual::Red:   return 72;
		default: PANIC();
		}
	};

	constexpr auto max_distance = duration_cast<nanoseconds>(1s);
	chart.upcoming_notes(max_distance, [&](auto const& note, bms::Chart::NoteType type, nanoseconds distance) {
		auto const visual = get_note_visual(type);
		auto const x = get_note_x(type);
		constexpr auto max_y = 539 + 6 - 13;
		auto const y = static_cast<int>(max_y - distance.count() / 1000000000.0 * max_y);
		auto const width = get_note_width(visual);
		auto const color = get_note_color(visual);
		queue.enqueue_rect({{x, y}, {width, 13}, color});
	});
}

export void render(threads::Broadcaster& broadcaster, dev::Window& window)
try {
	dev::name_current_thread("render");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<shared_ptr<bms::Chart>>();
	broadcaster.wait_for_others(3);
	auto gpu = dev::GPU{window};
	auto renderer = gfx::Renderer{gpu};

	auto chart = shared_ptr<bms::Chart>{};

	while (!window.is_closing()) {
		broadcaster.receive_all<shared_ptr<bms::Chart>>([&](auto&& recv) {
			chart = recv;
		});
		renderer.frame([&](gfx::Renderer::Queue& queue) {
			enqueue_test_scene(queue);
			if (chart) enqueue_chart_objects(queue, *chart);
		});
		FRAME_MARK();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
