/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio.cppm:
Initializes audio and handles queueing of playback events.
*/

module;
#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "io/file.hpp"

export module playnote.threads.audio;

import playnote.dev.window;
import playnote.dev.audio;
import playnote.dev.os;
import playnote.bms.audio_player;
import playnote.bms.build;
import playnote.bms.ir;
import playnote.threads.render_shouts;
import playnote.threads.audio_shouts;
import playnote.threads.input_shouts;
import playnote.threads.broadcaster;

namespace playnote::threads {

auto load_bms(bms::IRCompiler& compiler, fs::path const& path) -> bms::IR
{
	INFO("Loading BMS file \"{}\"", path.c_str());
	auto const file = io::read_file(path);
	auto ir = compiler.compile(path, file.contents);
	INFO("Loaded BMS file \"{}\" successfully", path.c_str());
	return ir;
}

void run(Broadcaster& broadcaster, dev::Window& window, dev::Audio& audio)
{
	auto chart_path = fs::path{};
	broadcaster.await<ChartLoadRequest>(
		[&](auto&& path) { chart_path = path; },
		[]() { yield(); });

	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::CompilingIR{chart_path});
	auto bms_compiler = bms::IRCompiler{};
	auto const bms_ir = load_bms(bms_compiler, chart_path);
	broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::Building{chart_path});
	auto const bms_chart = chart_from_ir(bms_ir, [&](auto& requests) {
		requests.process([&](string_view file, usize idx, usize total) {
			broadcaster.make_shout<ChartLoadProgress>(ChartLoadProgress::LoadingFile{
				.chart_path = chart_path,
				.filename = string{file},
				.index = idx,
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

export void audio(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window)
try {
	dev::name_current_thread("audio");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<PlayerControl>();
	broadcaster.subscribe<ChartLoadRequest>();
	barriers.startup.arrive_and_wait();
	auto audio = dev::Audio{};
	run(broadcaster, window, audio);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
