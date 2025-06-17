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
import playnote.gfx.playfield;
import playnote.gfx.renderer;
import playnote.bms.audio_player;
import playnote.bms.cursor;
import playnote.bms.chart;
import playnote.threads.render_events;
import playnote.threads.broadcaster;

namespace playnote::threads {

namespace im = lib::imgui;

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
	auto const chart_duration = ns_to_minsec(metrics.chart_duration);
	auto const audio_duration = ns_to_minsec(metrics.audio_duration);
	im::text("Progress: {} / {} ({})", progress, chart_duration, audio_duration);
	im::text("Notes: {} / {}", cursor.get_judged_notes(), metrics.note_count);
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
	im::input_float("Scroll speed", scroll_speed, 0.25f, 1.0f, "%.2f");
}

void run(Broadcaster& broadcaster, dev::Window const& window, gfx::Renderer& renderer)
{
	auto player = shared_ptr<bms::AudioPlayer const>{};
	auto playfield = optional<gfx::Playfield>{};
	auto scroll_speed = 2.0f;

	while (!window.is_closing()) {
		broadcaster.receive_all<weak_ptr<bms::AudioPlayer const>>([&](auto&& recv) {
			player = recv.lock();
			playfield = gfx::Playfield{{44, 0}, 545, player->get_chart().metrics.playstyle};
		});
		renderer.frame({"bg"_id, "frame"_id, "measure"_id, "judgment_line"_id, "notes"_id}, [&](gfx::Renderer::Queue& queue) {
			if (player) {
				queue.enqueue_rect("bg"_id, {{0, 0}, {1280, 720}, {0.060f, 0.060f, 0.060f, 1.000f}});
				auto const cursor = player->get_audio_cursor();
				auto const& chart = cursor.get_chart();
				im::begin_window("info", {860, 8}, 412, true);
				show_metadata(chart.metadata);
				im::text("");
				show_metrics(cursor, chart.metrics);
				im::text("");
				show_playback_controls(broadcaster);
				im::text("");
				show_scroll_speed_controls(scroll_speed);
				playfield->notes_from_cursor(cursor, scroll_speed);
				playfield->enqueue(queue);
				im::end_window();
			} else {
				im::begin_window("loading", {520, 300}, 240, true);
				im::text("Loading chart");
				im::end_window();
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
