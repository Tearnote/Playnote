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
#include "config.hpp"
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

static auto ns_to_minsec(nanoseconds duration) -> string
{
	return format("{}:{:02}", duration / 60s, duration % 60s / 1s);
}

static void show_metadata(bms::Cursor const& cursor, bms::Metadata const& meta)
{
	lib::imgui::text(meta.title);
	if (!meta.subtitle.empty()) lib::imgui::text(meta.subtitle);
	lib::imgui::text(meta.artist);
	if (!meta.subartist.empty()) lib::imgui::text(meta.subartist);
	lib::imgui::text(meta.genre);
	lib::imgui::text("Difficulty: {}", enum_name(meta.difficulty));
	if (!meta.url.empty()) lib::imgui::text(meta.url);
	if (!meta.email.empty()) lib::imgui::text(meta.email);

	lib::imgui::text("");

	auto const progress = ns_to_minsec(cursor.get_progress_ns());
	auto const chart_duration = ns_to_minsec(meta.chart_duration);
	auto const audio_duration = ns_to_minsec(meta.audio_duration);
	lib::imgui::text("Progress: {} / {} ({})", progress, chart_duration, audio_duration);
	lib::imgui::text("Notes: {} / {}", cursor.get_judged_notes(), meta.note_count);
	if (meta.bpm_range.main == meta.bpm_range.min && meta.bpm_range.main == meta.bpm_range.max)
		lib::imgui::text("BPM: {}", meta.bpm_range.main);
	else
		lib::imgui::text("BPM: {} - {} ({})", meta.bpm_range.min, meta.bpm_range.max, meta.bpm_range.main);
	lib::imgui::plot("Note density", {
		{"Scratch", meta.density.scratch, {1.0f, 0.1f, 0.1f, 1.0f}},
		{"LN", meta.density.ln, {0.1f, 0.1f, 1.0f, 1.0f}},
		{"Key", meta.density.key, {1.0f, 1.0f, 1.0f, 1.0f}},
	}, {
		{lib::imgui::PlotMarker::Type::Vertical, static_cast<float>(cursor.get_progress_ns() / 125ms), {1.0f, 0.0f, 0.0f, 1.0f}},
		{lib::imgui::PlotMarker::Type::Horizontal, meta.nps.average, {0.0f, 0.0f, 1.0f, 1.0f}},
		{lib::imgui::PlotMarker::Type::Horizontal, meta.nps.peak, {1.0f, 0.0f, 1.0f, 1.0f}}
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

static void show_scroll_speed_controls(double& scroll_speed)
{
	lib::imgui::input_double("Scroll speed", scroll_speed, 0.25f, 1.0f, "%.2f");
}

static void show_judgments(bms::Cursor::JudgeTotals judgments)
{
	lib::imgui::text("PGREAT: {}", judgments.types[+bms::Cursor::Judgment::Type::PGreat]);
	lib::imgui::text(" GREAT: {}", judgments.types[+bms::Cursor::Judgment::Type::Great]);
	lib::imgui::text("  GOOD: {}", judgments.types[+bms::Cursor::Judgment::Type::Good]);
	lib::imgui::text("   BAD: {}", judgments.types[+bms::Cursor::Judgment::Type::Bad]);
	lib::imgui::text("  POOR: {}", judgments.types[+bms::Cursor::Judgment::Type::Poor]);
}

static void show_earlylate(bms::Cursor::JudgeTotals judgments)
{
	lib::imgui::text(" Early: {}", judgments.timings[+bms::Cursor::Judgment::Timing::Early]);
	lib::imgui::text("  Late: {}", judgments.timings[+bms::Cursor::Judgment::Timing::Late]);
}

static void show_results(bms::Cursor const& cursor)
{
	lib::imgui::text("Score: {}", cursor.get_score());
	lib::imgui::text("Combo: {}", cursor.get_combo());
	lib::imgui::text(" Rank: {}", enum_name(cursor.get_rank()));
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
	lib::imgui::begin_window("loading", {420, 300}, 440, lib::imgui::WindowStyle::Static);
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
	auto scroll_speed = globals::config->get_entry<double>("gameplay", "scroll_speed");
	auto offset = milliseconds{globals::config->get_entry<int32>("gameplay", "note_offset")};

	while (!window.is_closing()) {
		receive_loading_shouts(broadcaster, loading_toast, [&](auto finished_player) {
			player = move(finished_player);
			playfield = gfx::Playfield{{44, 0}, 545, player->get_chart().timeline.playstyle};
		});
		renderer.frame({"bg"_id, "frame"_id, "measure"_id, "judgment_line"_id, "notes"_id, "pressed"_id}, [&](gfx::Renderer::Queue& queue) {
			queue.enqueue_rect("bg"_id, {{0, 0}, {1280, 720}, {0.060f, 0.060f, 0.060f, 1.000f}});
			if (loading_toast) enqueue_loading_toast(*loading_toast);
			if (player) {
				auto const cursor = player->get_audio_cursor();
				auto const& chart = cursor.get_chart();
				lib::imgui::begin_window("info", {860, 8}, 412, lib::imgui::WindowStyle::Static);
				show_metadata(cursor, chart.metadata);
				lib::imgui::text("");
				show_playback_controls(broadcaster);
				lib::imgui::text("");
				show_scroll_speed_controls(scroll_speed);
				playfield->enqueue_from_cursor(queue, cursor, scroll_speed, offset);
				lib::imgui::end_window();

				lib::imgui::begin_window("judgements", {860, 436}, 120, lib::imgui::WindowStyle::Static);
				show_judgments(cursor.get_judge_totals());
				lib::imgui::end_window();

				lib::imgui::begin_window("earlylate", {860, 558}, 120, lib::imgui::WindowStyle::Static);
				show_earlylate(cursor.get_judge_totals());
				lib::imgui::end_window();

				lib::imgui::begin_window("results", {988, 436}, 120, lib::imgui::WindowStyle::Static);
				show_results(cursor);
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
