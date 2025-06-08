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
import playnote.lib.imgui;
import playnote.dev.window;
import playnote.dev.gpu;
import playnote.dev.os;
import playnote.gfx.renderer;
import playnote.bms.chart;
import playnote.threads.render_events;
import playnote.threads.broadcaster;

namespace playnote::threads {

namespace im = lib::imgui;

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
	using LaneType = bms::Chart::LaneType;
	enum class NoteVisual { White, Blue, Red };
	auto get_note_visual = [](LaneType type) {
		switch (type) {
		case LaneType::P1_KeyS:
		case LaneType::P2_KeyS:
			return NoteVisual::Red;
		case LaneType::P1_Key1:
		case LaneType::P1_Key3:
		case LaneType::P1_Key5:
		case LaneType::P1_Key7:
		case LaneType::P2_Key1:
		case LaneType::P2_Key3:
		case LaneType::P2_Key5:
		case LaneType::P2_Key7:
			return NoteVisual::White;
		case LaneType::P1_Key2:
		case LaneType::P1_Key4:
		case LaneType::P1_Key6:
		case LaneType::P2_Key2:
		case LaneType::P2_Key4:
		case LaneType::P2_Key6:
			return NoteVisual::Blue;
		default: PANIC();
		}
	};
	auto get_note_x = [](LaneType type) {
		switch (type) {
		case LaneType::P1_KeyS: return 44;
		case LaneType::P1_Key1: return 118;
		case LaneType::P1_Key2: return 160;
		case LaneType::P1_Key3: return 194;
		case LaneType::P1_Key4: return 236;
		case LaneType::P1_Key5: return 270;
		case LaneType::P1_Key6: return 312;
		case LaneType::P1_Key7: return 346;
		case LaneType::P2_Key1: return 478;
		case LaneType::P2_Key2: return 520;
		case LaneType::P2_Key3: return 554;
		case LaneType::P2_Key4: return 596;
		case LaneType::P2_Key5: return 630;
		case LaneType::P2_Key6: return 672;
		case LaneType::P2_Key7: return 706;
		case LaneType::P2_KeyS: return 748;
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

	constexpr auto max_distance = 1.0f;
	chart.upcoming_notes(max_distance, [&](auto const& note, LaneType type, float distance) {
		constexpr auto max_y = 539 + 6;
		auto const ln_height = holds_alternative<bms::Note::LN>(note.type)?
			static_cast<int>(get<bms::Note::LN>(note.type).height * max_y) :
			0;
		auto const visual = get_note_visual(type);
		auto const x = get_note_x(type);
		auto const y = static_cast<int>(max_y - distance * max_y) - ln_height;
		auto const y_overflow = max(y + ln_height - max_y, 0);
		auto const width = get_note_width(visual);
		auto const color = get_note_color(visual);
		queue.enqueue_rect({{x, y - 13}, {width, 13 + ln_height - y_overflow}, color});
	});
}

void show_metadata(bms::Chart::Metadata const& meta)
{
	if (meta.subtitle.empty())
		im::text("{}", meta.title);
	else
		im::text("{}\n{}", meta.title, meta.subtitle);
	if (meta.subartist.empty())
		im::text("{}", meta.artist);
	else
		im::text("{}\n{}", meta.artist, meta.subartist);
	im::text("{}", meta.genre);
	im::text("Difficulty: {}", bms::Chart::Metadata::to_str(meta.difficulty));
	if (!meta.url.empty()) im::text("{}", meta.url);
	if (!meta.email.empty()) im::text("{}", meta.email);
}

export void render(Broadcaster& broadcaster, dev::Window& window)
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
			if (chart) {
				show_metadata(chart->get_metadata());
				if (im::button("Play")) broadcaster.shout(PlayerControl::Play);
				if (im::button("Pause")) broadcaster.shout(PlayerControl::Pause);
				if (im::button("Restart")) broadcaster.shout(PlayerControl::Restart);
				enqueue_chart_objects(queue, *chart);
			}
		});
		FRAME_MARK();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
