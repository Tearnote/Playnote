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
#include "dev/os.hpp"
#include "gfx/playfield.hpp"
#include "gfx/renderer.hpp"
#include "audio/player.hpp"
#include "audio/mixer.hpp"
#include "bms/library.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"
#include "bms/score.hpp"
#include "bms/input.hpp"
#include "threads/render_shouts.hpp"
#include "threads/input_shouts.hpp"
#include "threads/task_pool.hpp"
#include "threads/tools.hpp"

namespace playnote::threads {

enum class State {
	None,
	Library,
	Gameplay,
};

struct LibraryContext {
	vector<bms::Library::ChartEntry> charts;
};

struct GameplayContext {
	shared_ptr<bms::Chart const> chart;
	shared_ptr<bms::Cursor> cursor;
	optional<bms::Score> score;
	audio::Player player;
	optional<gfx::Playfield> playfield;
	double scroll_speed;
	milliseconds offset;
};

struct GameState {
	State current;
	State requested;
	lib::openssl::MD5 requested_chart;
	variant<monostate, LibraryContext, GameplayContext> context;

	[[nodiscard]] auto library_context() -> LibraryContext& { return get<LibraryContext>(context); }
	[[nodiscard]] auto gameplay_context() -> GameplayContext& { return get<GameplayContext>(context); }
};

static auto ns_to_minsec(nanoseconds duration) -> string
{
	return format("{}:{:02}", duration / 60s, duration % 60s / 1s);
}

static void show_metadata(GameplayContext const& context)
{
	auto const& meta = context.chart->metadata;
	lib::imgui::text(meta.title);
	if (!meta.subtitle.empty()) lib::imgui::text(meta.subtitle);
	lib::imgui::text(meta.artist);
	if (!meta.subartist.empty()) lib::imgui::text(meta.subartist);
	lib::imgui::text(meta.genre);
	lib::imgui::text("Difficulty: {}", enum_name(meta.difficulty));
	if (!meta.url.empty()) lib::imgui::text(meta.url);
	if (!meta.email.empty()) lib::imgui::text(meta.email);

	lib::imgui::text("");

	auto const progress = ns_to_minsec(context.cursor->get_progress_ns());
	auto const chart_duration = ns_to_minsec(meta.chart_duration);
	auto const audio_duration = ns_to_minsec(meta.audio_duration);
	lib::imgui::text("Progress: {} / {} ({})", progress, chart_duration, audio_duration);
	lib::imgui::text("Notes: {} / {}", context.score->get_judged_notes(), meta.note_count);
	if (meta.bpm_range.main == meta.bpm_range.min && meta.bpm_range.main == meta.bpm_range.max)
		lib::imgui::text("BPM: {}", meta.bpm_range.main);
	else
		lib::imgui::text("BPM: {} - {} ({})", meta.bpm_range.min, meta.bpm_range.max, meta.bpm_range.main);
	lib::imgui::plot("Note density", {
		{"Scratch", meta.density.scratch, {1.0f, 0.1f, 0.1f, 1.0f}},
		{"LN", meta.density.ln, {0.1f, 0.1f, 1.0f, 1.0f}},
		{"Key", meta.density.key, {1.0f, 1.0f, 1.0f, 1.0f}},
	}, {
		{lib::imgui::PlotMarker::Type::Vertical,static_cast<float>(min(context.cursor->get_progress_ns(), meta.chart_duration) / 125ms), {1.0f, 0.0f, 0.0f, 1.0f}},
		{lib::imgui::PlotMarker::Type::Horizontal, meta.nps.average, {0.0f, 0.0f, 1.0f, 1.0f}},
		{lib::imgui::PlotMarker::Type::Horizontal, meta.nps.peak, {1.0f, 0.0f, 1.0f, 1.0f}}
	}, 120, true);
}

static void show_playback_controls(GameState& state)
{
	auto& context = state.gameplay_context();
	if (lib::imgui::button("Play")) context.player.resume();
	lib::imgui::same_line();
	if (lib::imgui::button("Pause")) context.player.pause();
	lib::imgui::same_line();
	if (lib::imgui::button("Restart")) {
		context.player.remove_cursor(context.cursor);
		context.cursor = make_shared<bms::Cursor>(context.chart, false);
		context.player.add_cursor(context.cursor, bms::Mapper{});
		context.score = bms::Score{*context.chart};
	}
	lib::imgui::same_line();
	if (lib::imgui::button("Autoplay")) {
		context.player.remove_cursor(context.cursor);
		context.cursor = make_shared<bms::Cursor>(context.chart, true);
		context.player.add_cursor(context.cursor, bms::Mapper{});
		context.score = bms::Score{*context.chart};
	}
	lib::imgui::same_line();
	if (lib::imgui::button("Back")) state.requested = State::Library;
}

static void show_scroll_speed_controls(double& scroll_speed)
{
	lib::imgui::input_double("Scroll speed", scroll_speed, 0.25f, 1.0f, "%.2f");
}

static void show_judgments(bms::Score::JudgeTotals const& judgments)
{
	lib::imgui::text("PGREAT: {}", judgments.types[+bms::Score::JudgmentType::PGreat]);
	lib::imgui::text(" GREAT: {}", judgments.types[+bms::Score::JudgmentType::Great]);
	lib::imgui::text("  GOOD: {}", judgments.types[+bms::Score::JudgmentType::Good]);
	lib::imgui::text("   BAD: {}", judgments.types[+bms::Score::JudgmentType::Bad]);
	lib::imgui::text("  POOR: {}", judgments.types[+bms::Score::JudgmentType::Poor]);
}

static void show_earlylate(bms::Score::JudgeTotals const& judgments)
{
	lib::imgui::text(" Early: {}", judgments.timings[+bms::Score::Timing::Early]);
	lib::imgui::text("  Late: {}", judgments.timings[+bms::Score::Timing::Late]);
}

static void show_results(bms::Score const& score)
{
	lib::imgui::text("Score: {}", score.get_score());
	lib::imgui::text("Combo: {}", score.get_combo());
	lib::imgui::text(" Rank: {}", enum_name(score.get_rank()));
}

static void render_library(gfx::Renderer::Queue&, GameState& state)
{
	auto& context = state.library_context();
	lib::imgui::begin_window("library", {8, 8}, 800, lib::imgui::WindowStyle::Static);
	if (context.charts.empty()) {
		lib::imgui::text("The library is empty. Drag a song folder or archive onto the game window to import.");
	} else {
		for (auto const& chart: context.charts) {
			if (lib::imgui::selectable(chart.title.c_str())) {
				state.requested = State::Gameplay;
				state.requested_chart = chart.md5;
			}
		}
	}
	lib::imgui::end_window();
}

static void render_gameplay(gfx::Renderer::Queue& queue, GameState& state)
{
	auto& context = state.gameplay_context();
	auto const cursor = context.player.get_audio_cursor(context.cursor);
	auto& score = *context.score;

	// Update scoring
	context.cursor->each_judgment_event([&](auto&& ev) { score.submit_judgment_event(move(ev)); });

	lib::imgui::begin_window("info", {860, 8}, 412, lib::imgui::WindowStyle::Static);
	show_metadata(context);
	lib::imgui::text("");
	show_playback_controls(state);
	lib::imgui::text("");
	show_scroll_speed_controls(context.scroll_speed);
	context.playfield->enqueue_from_cursor(queue, cursor, score, context.scroll_speed, context.offset);
	lib::imgui::end_window();

	lib::imgui::begin_window("judgements", {860, 436}, 120, lib::imgui::WindowStyle::Static);
	show_judgments(score.get_judge_totals());
	lib::imgui::end_window();

	lib::imgui::begin_window("earlylate", {860, 558}, 120, lib::imgui::WindowStyle::Static);
	show_earlylate(score.get_judge_totals());
	lib::imgui::end_window();

	lib::imgui::begin_window("results", {988, 436}, 120, lib::imgui::WindowStyle::Static);
	show_results(score);
	lib::imgui::end_window();
}

static void run_render(Tools& tools, dev::Window& window)
{
	// Init devices
	auto mixer_stub = globals::mixer.provide();
	auto renderer = gfx::Renderer{window};

	// Init game systems
	auto library = bms::Library{LibraryDBPath};
	auto task_pool_stub = globals::task_pool.provide(coro::thread_pool::make_unique());
	auto state = GameState{};
	state.requested = State::Library;

	while (!window.is_closing()) {
		// Handle state changes
		if (state.requested == State::Library) {
			if (holds_alternative<GameplayContext>(state.context)) {
				tools.broadcaster.shout(UnregisterInputQueue{
					.queue = weak_ptr{state.gameplay_context().player.get_input_queue()},
				});
			}
			state.context.emplace<LibraryContext>();
			state.library_context().charts = library.list_charts(); //TODO convert to coro
			state.current = State::Library;
			state.requested = State::None;
		}
		if (state.requested == State::Gameplay) {
			state.context.emplace<GameplayContext>();
			auto& context = state.gameplay_context();
			context.chart = library.load_chart(state.requested_chart); //TODO convert to coro
			context.cursor = make_shared<bms::Cursor>(context.chart, false);
			context.score = bms::Score{*context.chart};
			tools.broadcaster.shout(RegisterInputQueue{
				.queue = weak_ptr{context.player.get_input_queue()},
			});
			context.player.add_cursor(context.cursor, bms::Mapper{});
			context.playfield = gfx::Playfield{{44, 0}, 545, context.cursor->get_chart().metadata.playstyle};
			context.scroll_speed = globals::config->get_entry<double>("gameplay", "scroll_speed"),
			context.offset = milliseconds{globals::config->get_entry<int32>("gameplay", "note_offset")};
			state.current = State::Gameplay;
			state.requested = State::None;
		}

		// Handle chart library
		tools.broadcaster.receive_all<FileDrop>([&](auto const& ev) {
			for (auto const& path: ev.paths) globals::pool().spawn(library.import(path));
		});
		if (state.current == State::Library && library.is_dirty()) state.library_context().charts = library.list_charts(); //TODO convert to coro

		// Render a frame
		renderer.frame({"bg"_id, "frame"_id, "measure"_id, "judgment_line"_id, "notes"_id, "pressed"_id}, [&](gfx::Renderer::Queue& queue) {
			// Background
			queue.enqueue_rect("bg"_id, {{0, 0}, {1280, 720}, {0.060f, 0.060f, 0.060f, 1.000f}});

			switch (state.current) {
			case State::Library: render_library(queue, state); break;
			case State::Gameplay: render_gameplay(queue, state); break;
			default: break;
			}
		});
	}
}

void render(Tools& tools, dev::Window& window)
try {
	dev::name_current_thread("render");
	tools.broadcaster.register_as_endpoint();
	tools.broadcaster.subscribe<FileDrop>();
	tools.barriers.startup.arrive_and_wait();
	run_render(tools, window);
	tools.barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	tools.barriers.shutdown.arrive_and_wait();
}

}
