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
import playnote.dev.audio;
import playnote.dev.gpu;
import playnote.dev.os;
import playnote.gfx.renderer;
import playnote.bms.audio_player;
import playnote.bms.cursor;
import playnote.bms.chart;
import playnote.threads.render_events;
import playnote.threads.broadcaster;

namespace playnote::threads {

namespace im = lib::imgui;

void enqueue_frame(gfx::Renderer::Queue& queue)
{
	queue.enqueue_rect("frame"_id, {{ 44, 539}, {342,   6}, {1.000f, 0.200f, 0.200f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{ 42,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{ 42, 545}, {346,   2}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{386,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{116,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{158,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{192,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{234,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{268,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{310,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{344,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{118,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{194,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{270,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{346,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});

	queue.enqueue_rect("frame"_id, {{478, 539}, {342,   6}, {1.000f, 0.200f, 0.200f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{476,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{476, 545}, {346,   2}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{820,   0}, {  2, 547}, {0.596f, 0.596f, 0.596f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{518,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{552,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{594,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{628,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{670,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{704,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{746,   0}, {  2, 539}, {0.165f, 0.165f, 0.165f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{478,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{554,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{630,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
	queue.enqueue_rect("frame"_id, {{706,   0}, { 40, 539}, {0.035f, 0.035f, 0.035f, 1.000f}});
}

void enqueue_chart_objects(gfx::Renderer::Queue& queue, bms::Cursor const& cursor, float scroll_speed)
{
	using LaneType = bms::Chart::LaneType;
	enum class NoteVisual { White, Blue, Red, Measure };
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
		case LaneType::MeasureLine:
			return NoteVisual::Measure;
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
		case NoteVisual::White:   return vec4{0.800f, 0.800f, 0.800f, 1.000f};
		case NoteVisual::Blue:    return vec4{0.200f, 0.600f, 0.800f, 1.000f};
		case NoteVisual::Red:     return vec4{0.800f, 0.200f, 0.200f, 1.000f};
		case NoteVisual::Measure: return vec4{0.267f, 0.267f, 0.267f, 1.000f};
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

	scroll_speed /= 4; // beat -> standard measure
	auto const max_distance = 1.0f / scroll_speed;
	cursor.upcoming_notes(max_distance, [&](auto const& note, LaneType type, float distance) {
		constexpr auto max_y = 539 + 6;
		auto const visual = get_note_visual(type);
		auto const color = get_note_color(visual);
		if (type == LaneType::MeasureLine) {
			auto const y = static_cast<int>(max_y - distance * max_y * scroll_speed) - 1;
			queue.enqueue_rect("measure"_id, {{44, y}, {342, 1}, color});
			queue.enqueue_rect("measure"_id, {{478, y}, {342, 1}, color});
			return;
		}
		auto const ln_height = holds_alternative<bms::Note::LN>(note.type)?
			static_cast<int>(get<bms::Note::LN>(note.type).height * max_y * scroll_speed) :
			0;
		auto const x = get_note_x(type);
		auto const y = static_cast<int>(max_y - distance * max_y * scroll_speed) - ln_height;
		auto const y_overflow = max(y + ln_height - max_y, 0);
		auto const width = get_note_width(visual);
		queue.enqueue_rect("notes"_id, {{x, y - 13}, {width, 13 + ln_height - y_overflow}, color});
	});
}

void show_metadata(bms::Metadata const& meta)
{
	im::text(meta.title);
	if (!meta.subtitle.empty()) {
		im::same_line();
		im::text(meta.subtitle);
	}
	im::text(meta.artist);
	if (!meta.subartist.empty()) {
		im::same_line();
		im::text(meta.subartist);
	}
	im::text(meta.genre);
	im::text("Difficulty: {}", bms::Metadata::to_str(meta.difficulty));
	if (!meta.url.empty()) im::text(meta.url);
	if (!meta.email.empty()) im::text(meta.email);
}

auto ns_to_minsec(nanoseconds duration) -> string
{
	return format("{}:{:02}", duration / 60s, duration % 60s / 1s);
}

void show_metrics(bms::Cursor const& cursor, bms::Metrics const& metrics)
{
	auto const progress = ns_to_minsec(cursor.get_progress_ns());
	auto const audio_duration = ns_to_minsec(metrics.audio_duration);
	im::text("Progress: {} / {}", progress, audio_duration);
}

void show_playback_controls(Broadcaster& broadcaster)
{
	if (im::button("Play")) broadcaster.shout(PlayerControl::Play);
	im::same_line();
	if (im::button("Pause")) broadcaster.shout(PlayerControl::Pause);
	im::same_line();
	if (im::button("Restart")) broadcaster.shout(PlayerControl::Restart);
}

void show_scroll_speed_controls(float& scroll_speed)
{
	im::text("Scroll speed");
	im::same_line();
	im::input_float("##scroll_speed", scroll_speed, 0.25f, 1.0f, "%.2f");
}

void run(Broadcaster& broadcaster, dev::Window const& window, gfx::Renderer& renderer)
{
	auto player = shared_ptr<bms::AudioPlayer const>{};
	auto scroll_speed = 2.0f;

	while (!window.is_closing()) {
		broadcaster.receive_all<weak_ptr<bms::AudioPlayer const>>([&](auto&& recv) { player = recv.lock(); });
		renderer.frame({"frame"_id, "measure"_id, "notes"_id}, [&](gfx::Renderer::Queue& queue) {
			enqueue_frame(queue);
			if (player) {
				auto const cursor = player->get_audio_cursor();
				show_metadata(player->get_chart().metadata);
				im::text("");
				show_metrics(cursor, player->get_chart().metrics);
				im::text("");
				show_playback_controls(broadcaster);
				im::text("");
				show_scroll_speed_controls(scroll_speed);
				enqueue_chart_objects(queue, cursor, scroll_speed);
			}
		});
		FRAME_MARK();
	}
}

export void render(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("render");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<weak_ptr<bms::AudioPlayer const>>();
	barriers.startup.arrive_and_wait();
	auto gpu = dev::GPU{window};
	auto renderer = gfx::Renderer{gpu};
	run(broadcaster, window, renderer);
	broadcaster.clear();
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	broadcaster.clear();
	barriers.shutdown.arrive_and_wait();
}

}
