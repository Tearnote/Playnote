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
#include "dev/audio.hpp"
#include "dev/os.hpp"
#include "io/file.hpp"
#include "bms/audio_player.hpp"
#include "bms/build.hpp"
#include "bms/ir.hpp"
#include "threads/render_shouts.hpp"
#include "threads/audio_shouts.hpp"
#include "threads/input_shouts.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

static auto load_bms(bms::IRCompiler& compiler, fs::path const& path) -> bms::IR
{
	INFO("Loading BMS file \"{}\"", path.c_str());
	auto const file = io::read_file(path);
	auto ir = compiler.compile(path, file.contents);
	INFO("Loaded BMS file \"{}\" successfully", path.c_str());
	return ir;
}

static void run_audio(Broadcaster& broadcaster, dev::Window& window, dev::Audio& audio)
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
	auto bms_player = make_shared<bms::AudioPlayer>(window.get_glfw(), audio);
	bms_player->play(*bms_chart);
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Finished{
		.chart_path = chart_path,
		.player = weak_ptr{bms_player},
	});

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
				bms_player->play(*bms_chart);
				break;
			default: PANIC();
			}
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
	barriers.startup.arrive_and_wait();
	auto audio = dev::Audio{};
	run_audio(broadcaster, window, audio);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
