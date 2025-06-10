/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio.cppm:
Initializes audio and handles queueing of playback events.
*/

module;
#include "macros/logger.hpp"
#include "macros/assert.hpp"

export module playnote.threads.audio;

import playnote.preamble;
import playnote.logger;
import playnote.io.audio_codec;
import playnote.io.file;
import playnote.dev.window;
import playnote.dev.audio;
import playnote.dev.os;
import playnote.bms.chart;
import playnote.bms.build;
import playnote.bms.ir;
import playnote.threads.render_events;
import playnote.threads.input_events;
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

export void audio(Broadcaster& broadcaster, dev::Window& window)
try {
	dev::name_current_thread("audio");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<PlayerControl>();
	broadcaster.subscribe<ChartLoadRequest>();
	broadcaster.wait_for_others(3);

	auto audio = dev::Audio{};
	io::AudioCodec::sampling_rate = audio.get_sampling_rate();
	auto chart_path = fs::path{};
	broadcaster.await<ChartLoadRequest>(
		[&](auto&& path) { chart_path = path; },
		[]() { yield(); });

	auto bms_compiler = bms::IRCompiler{};
	auto const bms_ir = load_bms(bms_compiler, chart_path);
	auto bms_chart = chart_from_ir(bms_ir, [](auto& requests) { requests.process(); });
	auto bms_cursor = make_shared<bms::Cursor>(*bms_chart);
	audio.play_chart(bms_cursor);
	broadcaster.shout(bms_cursor);
	while (!window.is_closing()) {
		broadcaster.receive_all<PlayerControl>([&](auto ev) {
			switch (ev) {
			case PlayerControl::Play:
				audio.resume();
				break;
			case PlayerControl::Pause:
				audio.pause();
				break;
			case PlayerControl::Restart:
				bms_cursor->restart();
				audio.play_chart(bms_cursor);
				break;
			default: PANIC();
			}
		});
		yield();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
