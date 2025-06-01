/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio.cppm:
Initializes audio and handles queueing of playback events.
*/

module;
#include "macros/logger.hpp"

export module playnote.threads.audio;

import playnote.preamble;
import playnote.logger;
import playnote.io.file;
import playnote.dev.window;
import playnote.dev.audio;
import playnote.dev.os;
import playnote.bms.chart;
import playnote.bms.ir;
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

export void audio(threads::Broadcaster& broadcaster, dev::Window& window)
try {
	dev::name_current_thread("audio");
	broadcaster.register_as_endpoint();
	broadcaster.wait_for_others(3);

	auto audio = dev::Audio{};
	auto bms_compiler = bms::IRCompiler{};
	auto const bms_ir = load_bms(bms_compiler, "songs/Ling Child/12_dphyper.bme");
//	auto const bms_jp = load_bms(bms_compiler, "songs/地球塔デヴォーション/DVTN_0708_SPH.bme");
//	auto const bms_kr = load_bms(bms_compiler, "songs/kkotbat ui norae ~ song of flower bed/sofb_5h (2).bms");
//	auto const bms_kr2 = load_bms(bms_compiler, "songs/Seoul Subway Song/sss_7h.bme");
	auto bms_chart = bms::Chart::from_ir(bms_ir);
	auto bulk_request = bms_chart.make_file_requests();
	bulk_request.process();
	audio.play_chart(bms_chart);
	while (!window.is_closing())
		yield();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
