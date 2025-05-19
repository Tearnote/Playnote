/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio_thread.cppm:
Initializes audio and handles queueing of playback events.
*/

module;
#include <type_traits>
#include <string_view>
#include <thread>
#include "util/log_macros.hpp"

export module playnote.audio_thread;

import playnote.stx.types;
import playnote.util.charset;
import playnote.util.file;
import playnote.sys.window;
import playnote.sys.audio;
import playnote.sys.os;
import playnote.bms.ir;
import playnote.globals;

namespace playnote {

using util::to_utf8;

/*
auto load_audio_file(std::string_view path) -> std::vector<float>
{
	auto file_info = SF_INFO{};
	auto* file = sf_open(std::string{path}.c_str(), SFM_READ, &file_info);
	if (!file) {
		L_ERROR("Failed to open assets/test.ogg: {}", sf_strerror(nullptr));
		return {};
	}
	ASSUME(file_info.channels == 2);
	auto samples_raw = std::vector<float>{};
	samples_raw.resize(file_info.frames * file_info.channels);
	sf_readf_float(file, samples_raw.data(), file_info.frames);
	sf_close(file);

	if (file_info.samplerate == 48000) {
		return std::move(samples_raw);
	} else {
		auto ratio = 48000.0 / static_cast<double>(file_info.samplerate);
		auto output_frame_count = static_cast<long>(std::ceil(file_info.frames * ratio));
		output_frame_count = output_frame_count + 8 - (output_frame_count % 8);
		auto samples = std::vector<float>{};
		samples.resize(output_frame_count * file_info.channels);

		auto src_data = SRC_DATA{
			.data_in = samples_raw.data(),
			.data_out = samples.data(),
			.input_frames = file_info.frames,
			.output_frames = output_frame_count,
			.src_ratio = ratio,
		};
		auto ret = src_simple(&src_data, SRC_SINC_BEST_QUALITY, file_info.channels);
		if (ret != 0) {
			L_ERROR("Failed to resample audio: {}", src_strerror(ret));
		}
		samples.resize(src_data.output_frames_gen * file_info.channels);
		return std::move(samples);
	}
}
*/
auto load_bms(bms::IRCompiler& compiler, std::string_view path) -> bms::IR
{
	L_INFO("Loading BMS file \"{}\"", path);
	auto file = util::read_file(path);
	auto ir = compiler.compile(path, file.contents);
	L_INFO("Loaded BMS file \"{}\" successfully", path);
	return ir;
}

export void audio_thread(sys::Window& window, int argc, char* argv[])
try {
	sys::set_thread_name("audio");

	auto audio = sys::Audio{argc, argv};
	auto bms_compiler = bms::IRCompiler{};
	auto bms_ir = load_bms(bms_compiler, "songs/Ling Child/02_hyper.bme");
	while (!window.is_closing())
		std::this_thread::yield();
}
catch (std::exception const& e) {
	L_CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
