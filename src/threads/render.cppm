/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.cppm:
Presents current game state onto the window at the screen's refresh rate.
*/

module;
#include "gfx/renderer.hpp"
#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "lib/imgui.hpp"
#include "lib/tracy.hpp"
#include "dev/window.hpp"
#include "dev/audio.hpp"
#include "dev/gpu.hpp"
#include "dev/os.hpp"
#include "bms/audio_player.hpp"
#include "bms/cursor.hpp"
#include "threads/audio_shouts.hpp"

export module playnote.threads.render;

import playnote.gfx.playfield;
import playnote.bms.chart;
import playnote.threads.render_shouts;
import playnote.threads.broadcaster;

namespace playnote::threads {

namespace im = lib::imgui;

void show_metadata(bms::Metadata const& meta)
{
	im::text(meta.title);
	if (!meta.subtitle.empty()) im::text(meta.subtitle);
	im::text(meta.artist);
	if (!meta.subartist.empty()) im::text(meta.subartist);
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
	im::plot("Note density", {
		{"Scratch", metrics.density.scratch_density, {1.0f, 0.1f, 0.1f, 1.0f}},
		{"LN", metrics.density.ln_density, {0.1f, 0.1f, 1.0f, 1.0f}},
		{"Key", metrics.density.key_density, {1.0f, 1.0f, 1.0f, 1.0f}},
	}, {
		{im::PlotMarker::Type::Vertical, static_cast<float>(cursor.get_progress_ns() / 125ms), {1.0f, 0.0f, 0.0f, 1.0f}}
	}, 120, true);
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

struct LoadingToast {
	fs::path path;
	string phase;
	optional<float> progress;
	optional<string> progress_text;
};

template<callable<void(shared_ptr<bms::AudioPlayer const>)> Func>
void receive_loading_shouts(Broadcaster& broadcaster, optional<LoadingToast>& loading_toast, Func&& on_finish)
{
	broadcaster.receive_all<ChartLoadProgress>([&](auto&& recv) {
		visit(visitor {
			[&](ChartLoadProgress::CompilingIR& msg) {
				if (!loading_toast) loading_toast.emplace();
				loading_toast->path = msg.chart_path;
				loading_toast->phase = "Compiling IR";
				loading_toast->progress = nullopt;
				loading_toast->progress_text = nullopt;
			},
			[&](ChartLoadProgress::Building& msg) {
				if (!loading_toast) loading_toast.emplace();
				loading_toast->path = msg.chart_path;
				loading_toast->phase = "Building";
				loading_toast->progress = nullopt;
				loading_toast->progress_text = nullopt;
			},
			[&](ChartLoadProgress::LoadingFile& msg) {
				if (!loading_toast) loading_toast.emplace();
				loading_toast->path = msg.chart_path;
				loading_toast->phase = "Loading files";
				loading_toast->progress = static_cast<float>(msg.index) / static_cast<float>(msg.total);
				loading_toast->progress_text = format("{} / {}", msg.index, msg.total);
			},
			[&](ChartLoadProgress::Measuring& msg) {
				if (!loading_toast) loading_toast.emplace();
				loading_toast->path = msg.chart_path;
				loading_toast->phase = "Measuring loudness";
				loading_toast->progress = nullopt;
				loading_toast->progress_text = ns_to_minsec(msg.progress);
			},
			[&](ChartLoadProgress::DensityCalculation& msg) {
				if (!loading_toast) loading_toast.emplace();
				loading_toast->path = msg.chart_path;
				loading_toast->phase = "Calculating density";
				loading_toast->progress = nullopt;
				loading_toast->progress_text = ns_to_minsec(msg.progress);
			},
			[&](ChartLoadProgress::Finished& msg) {
				loading_toast.reset();
				on_finish(msg.player.lock());
			},
			[](auto&&) {}
		}, recv.type);
	});
}

void enqueue_loading_toast(LoadingToast const& toast)
{
	im::begin_window("loading", {420, 300}, 440, true);
	im::text("Loading {}", toast.path);
	im::text(toast.phase);
	if (toast.progress_text) {
		im::progress_bar(toast.progress, *toast.progress_text);
	}
	im::end_window();
}

void run(Broadcaster& broadcaster, dev::Window const& window, gfx::Renderer& renderer)
{
	auto player = shared_ptr<bms::AudioPlayer const>{};
	auto playfield = optional<gfx::Playfield>{};
	auto loading_toast = optional<LoadingToast>{};
	auto scroll_speed = 2.0f;

	while (!window.is_closing()) {
		receive_loading_shouts(broadcaster, loading_toast, [&](auto finished_player) {
			player = move(finished_player);
			playfield = gfx::Playfield{{44, 0}, 545, player->get_chart().metrics.playstyle};
		});
		renderer.frame({"bg"_id, "frame"_id, "measure"_id, "judgment_line"_id, "notes"_id}, [&](gfx::Renderer::Queue& queue) {
			queue.enqueue_rect("bg"_id, {{0, 0}, {1280, 720}, {0.060f, 0.060f, 0.060f, 1.000f}});
			if (loading_toast) enqueue_loading_toast(*loading_toast);
			if (player) {
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
			}
		});
		FRAME_MARK();
	}
}

export void render(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("render");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<ChartLoadProgress>();
	barriers.startup.arrive_and_wait();
	auto gpu = dev::GPU{window};
	auto renderer = gfx::Renderer{gpu};
	run(broadcaster, window, renderer);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
