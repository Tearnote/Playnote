/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.cpp:
Implementation file for threads/render.hpp.
*/

#include "threads/render.hpp"

#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "lib/imgui.hpp"
#include "dev/window.hpp"
#include "dev/audio.hpp"
#include "dev/os.hpp"
#include "gfx/playfield.hpp"
#include "gfx/renderer.hpp"
#include "audio/player.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"
#include "threads/render_shouts.hpp"
#include "threads/audio_shouts.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

static void show_metadata(bms::Metadata const& meta)
{
	lib::imgui::text(meta.title);
	if (!meta.subtitle.empty()) lib::imgui::text(meta.subtitle);
	lib::imgui::text(meta.artist);
	if (!meta.subartist.empty()) lib::imgui::text(meta.subartist);
	lib::imgui::text(meta.genre);
	lib::imgui::text("Difficulty: {}", bms::Metadata::to_str(meta.difficulty));
	if (!meta.url.empty()) lib::imgui::text(meta.url);
	if (!meta.email.empty()) lib::imgui::text(meta.email);
}

static auto ns_to_minsec(nanoseconds duration) -> string
{
	return format("{}:{:02}", duration / 60s, duration % 60s / 1s);
}

static void show_metrics(bms::Cursor const& cursor, bms::Metrics const& metrics)
{
	auto const progress = ns_to_minsec(cursor.get_progress_ns());
	auto const chart_duration = ns_to_minsec(metrics.chart_duration);
	auto const audio_duration = ns_to_minsec(metrics.audio_duration);
	lib::imgui::text("Progress: {} / {} ({})", progress, chart_duration, audio_duration);
	lib::imgui::text("Notes: {} / {}", cursor.get_judged_notes(), metrics.note_count);
	lib::imgui::plot("Note density", {
		{"Scratch", metrics.density.scratch_density, {1.0f, 0.1f, 0.1f, 1.0f}},
		{"LN", metrics.density.ln_density, {0.1f, 0.1f, 1.0f, 1.0f}},
		{"Key", metrics.density.key_density, {1.0f, 1.0f, 1.0f, 1.0f}},
	}, {
		{lib::imgui::PlotMarker::Type::Vertical, static_cast<float>(cursor.get_progress_ns() / 125ms), {1.0f, 0.0f, 0.0f, 1.0f}},
		{lib::imgui::PlotMarker::Type::Horizontal, metrics.density.average_nps, {0.0f, 0.0f, 1.0f, 1.0f}},
		{lib::imgui::PlotMarker::Type::Horizontal, metrics.density.peak_nps, {1.0f, 0.0f, 1.0f, 1.0f}}
	}, 120, true);
}

static void show_playback_controls(Broadcaster& broadcaster)
{
	if (lib::imgui::button("Play")) broadcaster.shout(PlayerControl::Play);
	lib::imgui::same_line();
	if (lib::imgui::button("Pause")) broadcaster.shout(PlayerControl::Pause);
	lib::imgui::same_line();
	if (lib::imgui::button("Restart")) broadcaster.shout(PlayerControl::Restart);
	lib::imgui::same_line();
	if (lib::imgui::button("Autoplay")) broadcaster.shout(PlayerControl::Autoplay);
}

static void show_scroll_speed_controls(float& scroll_speed)
{
	lib::imgui::input_float("Scroll speed", scroll_speed, 0.25f, 1.0f, "%.2f");
}

static void show_judgments(bms::Cursor::Judgments judgments)
{
	lib::imgui::text("PGREAT: {}", judgments.pgreat);
	lib::imgui::text(" GREAT: {}", judgments.great);
	lib::imgui::text("  GOOD: {}", judgments.good);
	lib::imgui::text("   BAD: {}", judgments.bad);
	lib::imgui::text("  POOR: {}", judgments.poor);
}

static void show_earlylate(bms::Cursor::Judgments judgments)
{
	lib::imgui::text(" Early: {}", judgments.early);
	lib::imgui::text("  Late: {}", judgments.late);
}

struct LoadingToast {
	fs::path path;
	string phase;
	optional<float> progress;
	optional<string> progress_text;
};

template<callable<void(shared_ptr<audio::Player const>)> Func>
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
			[&](ChartLoadProgress::LoadingFiles& msg) {
				if (!loading_toast) loading_toast.emplace();
				loading_toast->path = msg.chart_path;
				loading_toast->phase = "Loading files";
				loading_toast->progress = static_cast<float>(msg.loaded) / static_cast<float>(msg.total);
				loading_toast->progress_text = format("{} / {}", msg.loaded, msg.total);
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
			[&](ChartLoadProgress::Failed& msg) {
				loading_toast->path = msg.chart_path;
				loading_toast->phase = format("Loading failed\n{}", msg.message);
				loading_toast->progress = nullopt;
				loading_toast->progress_text = nullopt;
			},
			[](auto&&) {}
		}, recv.type);
	});
}

static void enqueue_loading_toast(LoadingToast const& toast)
{
	lib::imgui::begin_window("loading", {420, 300}, 440, true);
	lib::imgui::text("Loading {}", toast.path);
	lib::imgui::text(toast.phase);
	if (toast.progress_text) {
		lib::imgui::progress_bar(toast.progress, *toast.progress_text);
	}
	lib::imgui::end_window();
}

static void run_render(Broadcaster& broadcaster, dev::Window const& window, gfx::Renderer& renderer)
{
	auto player = shared_ptr<audio::Player const>{};
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
				lib::imgui::begin_window("info", {860, 8}, 412, true);
				show_metadata(chart.metadata);
				lib::imgui::text("");
				show_metrics(cursor, chart.metrics);
				lib::imgui::text("");
				show_playback_controls(broadcaster);
				lib::imgui::text("");
				show_scroll_speed_controls(scroll_speed);
				playfield->notes_from_cursor(cursor, scroll_speed);
				playfield->enqueue(queue);
				lib::imgui::end_window();

				lib::imgui::begin_window("judgements", {860, 436}, 120, true);
				show_judgments(cursor.get_judgments());
				lib::imgui::end_window();

				lib::imgui::begin_window("earlylate", {860, 558}, 120, true);
				show_earlylate(cursor.get_judgments());
				lib::imgui::end_window();
			}
		});
	}
}

void render(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("render");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<ChartLoadProgress>();
	barriers.startup.arrive_and_wait();
	auto renderer = gfx::Renderer{window};
	run_render(broadcaster, window, renderer);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
