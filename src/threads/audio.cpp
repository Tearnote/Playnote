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
#include "io/file.hpp"
#include "audio/player.hpp"
#include "audio/mixer.hpp"
#include "bms/build.hpp"
#include "bms/input.hpp"
#include "bms/ir.hpp"
#include "threads/render_shouts.hpp"
#include "threads/audio_shouts.hpp"
#include "threads/input_shouts.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

static auto load_bms(bms::IRCompiler& compiler, fs::path const& path) -> bms::IR
{
	INFO("Loading BMS file \"{}\"", path.string());
	auto const file = io::read_file(path);
	auto ir = compiler.compile(path, file.contents);
	INFO("Loaded BMS file \"{}\" successfully", path.string());
	return ir;
}

static void run_audio(Broadcaster& broadcaster, dev::Window& window, audio::Mixer& mixer)
{
	auto chart_path = fs::path{};
	broadcaster.await<ChartLoadRequest>(
		[&](auto&& path) { chart_path = path; },
		[]() { yield(); });

	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::CompilingIR{chart_path});
	auto bms_compiler = bms::IRCompiler{};
	auto try_bms_ir = optional<bms::IR>{};
	try {
		try_bms_ir.emplace(load_bms(bms_compiler, chart_path));
	} catch (exception const& e) {
		broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Failed{
			.chart_path = chart_path,
			.message = e.what(),
		});
		return;
	}
	auto bms_ir = move(*try_bms_ir);
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Building{chart_path});
	auto const bms_chart = chart_from_ir(bms_ir, [&](auto& requests) {
		requests.process([&](usize loaded, usize total) {
			broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::LoadingFiles{
				.chart_path = chart_path,
				.loaded = loaded,
				.total = total,
			});
		});
	}, [&](ChartLoadProgress::Type progress) {
		visit(visitor {
			[&](ChartLoadProgress::Measuring& msg) { msg.chart_path = chart_path; },
			[&](ChartLoadProgress::DensityCalculation& msg) { msg.chart_path = chart_path; },
			[](auto&&){}
		}, progress);
		broadcaster.make_shout<ChartLoadProgress>(move(progress));
	});
	auto bms_player = make_shared<audio::Player>(window.get_glfw(), mixer);
	bms_player->play(*bms_chart, false);
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Finished{
		.chart_path = chart_path,
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
			auto input = mapper.from_key(ev);
			if (!input) return;
			bms_player->enqueue_input(*input);
		});
		yield();
	}
}

void audio(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("audio");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<PlayerControl>();
	broadcaster.subscribe<ChartLoadRequest>();
	broadcaster.subscribe<KeyInput>();
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
