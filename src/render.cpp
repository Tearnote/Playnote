/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
except according to those terms.
*/

#include "render.hpp"

#include "preamble.hpp"
#include "utils/broadcaster.hpp"
#include "utils/task_pool.hpp"
#include "utils/logger.hpp"
#include "utils/config.hpp"
#include "lib/imgui.hpp"
#include "lib/os.hpp"
#include "dev/window.hpp"
#include "gfx/playfield.hpp"
#include "gfx/renderer.hpp"
#include "audio/player.hpp"
#include "audio/mixer.hpp"
#include "bms/library.hpp"
#include "bms/cursor.hpp"
#include "bms/mapper.hpp"
#include "bms/chart.hpp"
#include "bms/score.hpp"
#include "input.hpp"

namespace playnote {

enum class State {
	None,
	Select,
	Gameplay,
};

struct ImportStatus {
	bool complete;
	uint32 songs_processed;
	uint32 songs_total;
	uint32 songs_failed;
	uint32 charts_added;
	uint32 charts_skipped;
	uint32 charts_failed;
};

struct SelectContext {
	vector<bms::Library::ChartEntry> charts;
	optional<future<vector<bms::Library::ChartEntry>>> library_reload_result;
	optional<future<shared_ptr<bms::Chart const>>> chart_load_result;
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
	shared_ptr<bms::Library> library;
	variant<monostate, SelectContext, GameplayContext> context;
	optional<ImportStatus> import_status;

	[[nodiscard]] auto select_context() -> SelectContext& { return get<SelectContext>(context); }
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

	auto const cursor_pos = ratio(min(context.cursor->get_progress_ns(), meta.chart_duration), meta.density.resolution);
	lib::imgui::plot("Note density", {
		{"Scratch", meta.density.scratch, {1.0f, 0.1f, 0.1f, 1.0f}},
		{"LN", meta.density.ln, {0.1f, 0.1f, 1.0f, 1.0f}},
		{"Key", meta.density.key, {1.0f, 1.0f, 1.0f, 1.0f}},
	}, {
		{lib::imgui::PlotMarker::Type::Vertical, static_cast<float>(cursor_pos), {1.0f, 0.0f, 0.0f, 1.0f}},
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
	if (lib::imgui::button("Back")) state.requested = State::Select;
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

static void render_select(gfx::Renderer::Queue&, GameState& state)
{
	auto& context = state.select_context();
	lib::imgui::begin_window("library", {8, 8}, 800, lib::imgui::WindowStyle::Static);
	if (context.charts.empty()) {
		lib::imgui::text("The library is empty. Drag a song folder or archive onto the game window to import.");
	} else {
		for (auto const& chart: context.charts) {
			if (lib::imgui::selectable(chart.title.c_str())) {
				context.chart_load_result = launch_pollable(
					[](shared_ptr<bms::Library> library, bms::MD5 md5) -> task<shared_ptr<bms::Chart const>> {
						co_return co_await library->load_chart(md5);
					}(state.library, chart.md5));
				state.requested = State::Gameplay;
			}
		}
	}
	lib::imgui::end_window();

	if (context.chart_load_result) {
		lib::imgui::begin_window("chart_load", {860, 8}, 96, lib::imgui::WindowStyle::Static);
		lib::imgui::text("Loading...");
		lib::imgui::end_window();
	}
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

	lib::imgui::begin_window("results", {988, 436}, 120, lib::imgui::WindowStyle::Static);
	show_results(score);
	lib::imgui::end_window();

	lib::imgui::begin_window("earlylate", {1116, 436}, 120, lib::imgui::WindowStyle::Static);
	show_earlylate(score.get_judge_totals());
	lib::imgui::end_window();
}

static auto render_import_status(ImportStatus const& status) -> bool
{
	auto reset = false;
	lib::imgui::begin_window("import_status", {860, 560}, 412, lib::imgui::WindowStyle::Static);
	if (!status.complete)
		lib::imgui::text("Import in progress...");
	else
		lib::imgui::text("Import complete!");
	if (!status.complete)
		lib::imgui::text("Songs processed: {} / {}", status.songs_processed, status.songs_total);
	else
		lib::imgui::text("Songs processed: {}", status.songs_processed);
	if (status.songs_failed)
		lib::imgui::text_styled(format("Songs failed: {}", status.songs_failed), vec4{1.0f, 0.3f, 0.3f, 1.0f});
	lib::imgui::text("Charts added: {}", status.charts_added);
	if (status.charts_skipped)
		lib::imgui::text_styled(format("Charts skipped: {}", status.charts_skipped), vec4{0.4f, 0.4f, 0.4f, 1.0f});
	if (status.charts_failed)
		lib::imgui::text_styled(format("Charts failed: {}", status.charts_failed), vec4{1.0f, 0.3f, 0.3f, 1.0f});
	if (status.complete)
		if (lib::imgui::button("Okay")) reset = true;
	lib::imgui::end_window();
	return reset;
}

static void run_render(Broadcaster& broadcaster, dev::Window& window, Logger::Category cat)
{
	// Init subsystems
	auto task_pool_stub = globals::task_pool.provide(thread_pool::make_unique({
		.on_thread_start_functor = [](auto worker_idx) {
			lib::os::name_current_thread(format("pool_worker{}", worker_idx));
			lib::os::lower_current_thread_priority();
		},
	}));
	DEBUG_AS(cat, "Task pool initialized");
	auto audio_cat =  globals::logger->create_category("Audio",
		*enum_cast<Logger::Level>(globals::config->get_entry<string>("logging", "audio")));
	auto mixer_stub = globals::mixer.provide(audio_cat);
	auto renderer = gfx::Renderer{window, cat};

	// Init game state
	auto state = GameState{};
	auto library_cat = globals::logger->create_category("Library",
		*enum_cast<Logger::Level>(globals::config->get_entry<string>("logging", "library")));
	state.library = make_shared<bms::Library>(library_cat, LibraryDBPath);
	state.requested = State::Select;

	while (!window.is_closing()) {
		// Handle state changes
		if (state.requested == State::Select) {
			if (holds_alternative<GameplayContext>(state.context)) {
				broadcaster.shout(UnregisterInputQueue{
					.queue = weak_ptr{state.gameplay_context().player.get_input_queue()},
				});
			}
			state.context.emplace<SelectContext>();
			state.select_context().library_reload_result = launch_pollable(
				[](shared_ptr<bms::Library> library) -> task<vector<bms::Library::ChartEntry>> {
					co_return co_await library->list_charts();
				}(state.library));
			state.current = State::Select;
			state.requested = State::None;
		}
		if (state.requested == State::Gameplay && state.current == State::Select && state.select_context().chart_load_result &&
			state.select_context().chart_load_result->wait_for(0s) == future_status::ready)
		{
			auto chart = state.select_context().chart_load_result->get();
			state.context.emplace<GameplayContext>();
			auto& context = state.gameplay_context();
			context.chart = move(chart);
			context.cursor = make_shared<bms::Cursor>(context.chart, false);
			context.score = bms::Score{*context.chart};
			broadcaster.shout(RegisterInputQueue{
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
		broadcaster.receive_all<FileDrop>([&](auto const& ev) {
			for (auto const& path: ev.paths) state.library->import(path);
		});
		if (state.current == State::Select) {
			auto& context = state.select_context();
			if (state.library->is_dirty() && !context.library_reload_result) {
				state.select_context().library_reload_result = launch_pollable(
					[](shared_ptr<bms::Library> library) -> task<vector<bms::Library::ChartEntry>> {
						co_return co_await library->list_charts();
					}(state.library));
			}
			if (context.library_reload_result && context.library_reload_result->wait_for(0s) == future_status::ready) {
				context.charts = context.library_reload_result->get();
				context.library_reload_result = nullopt;
			}
		}
		if (state.library->is_importing()) {
			if (!state.import_status) state.import_status = ImportStatus{};
			state.import_status->complete = false;
			state.import_status->songs_processed = state.library->get_import_songs_processed();
			state.import_status->songs_total = state.library->get_import_songs_total();
			state.import_status->songs_failed = state.library->get_import_songs_failed();
			state.import_status->charts_added = state.library->get_import_charts_added();
			state.import_status->charts_skipped = state.library->get_import_charts_skipped();
			state.import_status->charts_failed = state.library->get_import_charts_failed();
		} else {
			if (state.import_status) {
				state.import_status->complete = true;
				state.import_status->songs_processed = state.library->get_import_songs_processed();
				state.import_status->songs_total = state.library->get_import_songs_total();
				state.import_status->songs_failed = state.library->get_import_songs_failed();
				state.import_status->charts_added = state.library->get_import_charts_added();
				state.import_status->charts_skipped = state.library->get_import_charts_skipped();
				state.import_status->charts_failed = state.library->get_import_charts_failed();
			}
		}

		// Render a frame
		renderer.frame({"bg"_id, "frame"_id, "measure"_id, "judgment_line"_id, "notes"_id, "pressed"_id}, [&](gfx::Renderer::Queue& queue) {
			// Background
			queue.enqueue_rect("bg"_id, {{0, 0}, {1280, 720}, {0.060f, 0.060f, 0.060f, 1.000f}});

			switch (state.current) {
			case State::Select: render_select(queue, state); break;
			case State::Gameplay: render_gameplay(queue, state); break;
			default: break;
			}
			if (state.import_status) {
				if (render_import_status(*state.import_status)) {
					state.library->reset_import_stats();
					state.import_status = nullopt;
				}
			}
		});
	}
}

void render_thread(Broadcaster& broadcaster, Barriers<2>& barriers, dev::Window& window)
try {
	lib::os::name_current_thread("render");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<FileDrop>();
	barriers.startup.arrive_and_wait();
	auto cat = globals::logger->create_category("Render",
		*enum_cast<Logger::Level>(globals::config->get_entry<string>("logging", "render")));
	run_render(broadcaster, window, cat);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	auto cat = globals::logger->create_category("Render");
	CRIT_AS(cat, "Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
