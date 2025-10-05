/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio.cpp:
Implementation file for threads/audio.hpp.
*/

#include "threads/audio.hpp"

#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "io/song_legacy.hpp"
#include "audio/player.hpp"
#include "audio/mixer.hpp"
#include "bms/library.hpp"
#include "bms/build_legacy.hpp"
#include "bms/input.hpp"
#include "bms/ir_legacy.hpp"
#include "threads/render_shouts.hpp"
#include "threads/audio_shouts.hpp"
#include "threads/input_shouts.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

static auto load_bms(bms::IRCompiler& compiler, io::SongLegacy const& song, string_view filename) -> bms::IR
{
	INFO("Loading BMS file \"{}\"", filename);
	auto const file = song.load_bms(filename);
	auto ir = compiler.compile(file);
	INFO("Loaded BMS file \"{}\" successfully", filename);
	return ir;
}

static void run_audio(Broadcaster& broadcaster, dev::Window& window, audio::Mixer& mixer)
{
	auto request = ChartRequest{};
	broadcaster.await<ChartRequest>(
		[&](auto&& req) { request = move(req); },
		[]() { yield(); });

	auto library = bms::Library{LibraryDBPath};
	auto song = io::SongLegacy{request.domain};
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::CompilingIR{request.filename});
	auto bms_compiler = bms::IRCompiler{};
	auto try_bms_ir = optional<bms::IR>{};
	try {
		try_bms_ir.emplace(load_bms(bms_compiler, song, request.filename));
	} catch (exception const& e) {
		broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Failed{
			.chart_path = request.filename,
			.message = e.what(),
		});
		return;
	}
	auto bms_ir = move(*try_bms_ir);
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Building{request.filename});
	auto const bms_chart = chart_from_ir(bms_ir, song, [&](ChartLoadProgress::Type progress) {
		visit(visitor {
			[&](ChartLoadProgress::LoadingFiles& msg) { msg.chart_path = request.filename; },
			[&](ChartLoadProgress::Measuring& msg) { msg.chart_path = request.filename; },
			[&](ChartLoadProgress::DensityCalculation& msg) { msg.chart_path = request.filename; },
			[](auto&&){}
		}, progress);
		broadcaster.make_shout<ChartLoadProgress>(move(progress));
	});
	auto bms_player = make_shared<audio::Player>(window.get_glfw(), mixer);
	bms_player->play(*bms_chart, false);
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Finished{
		.chart_path = request.filename,
		.player = weak_ptr{bms_player},
	});
	auto mapper = bms::Mapper{};

	while (!window.is_closing()) {
		broadcaster.receive_all<PlayerControl>([&](auto ev) {
			switch (ev) {
			case PlayerControl::Play:
				bms_player->resume();
				break;
			case PlayerControl::Pause:
				bms_player->pause();
				break;
			case PlayerControl::Restart:
				bms_player->play(*bms_chart, false);
				break;
			case PlayerControl::Autoplay:
				bms_player->play(*bms_chart, true);
				break;
			default: PANIC();
			}
		});
		broadcaster.receive_all<KeyInput>([&](auto ev) {
			auto input = mapper.from_key(ev, bms_player->get_chart().metadata.playstyle);
			if (!input) return;
			bms_player->enqueue_input(*input);
		});
		broadcaster.receive_all<ButtonInput>([&](auto ev) {
			auto input = mapper.from_button(ev, bms_player->get_chart().metadata.playstyle);
			if (!input) return;
			bms_player->enqueue_input(*input);
		});
		broadcaster.receive_all<AxisInput>([&](auto ev) {
			auto inputs = mapper.submit_axis_input(ev, bms_player->get_chart().metadata.playstyle);
			for (auto const& input: inputs) bms_player->enqueue_input(input);
		});
		broadcaster.receive_all<FileDrop>([&](auto ev) {
			for (auto const& path: ev.paths) library.import(path);
		});
		auto inputs = mapper.from_axis_state(window.get_glfw(), bms_player->get_chart().metadata.playstyle);
		for (auto const& input: inputs) bms_player->enqueue_input(input);

		yield();
	}
}

void audio(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("audio");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<PlayerControl>();
	broadcaster.subscribe<ChartRequest>();
	broadcaster.subscribe<KeyInput>();
	broadcaster.subscribe<ButtonInput>();
	broadcaster.subscribe<AxisInput>();
	broadcaster.subscribe<FileDrop>();
	barriers.startup.arrive_and_wait();
	auto mixer = audio::Mixer{};
	run_audio(broadcaster, window, mixer);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
