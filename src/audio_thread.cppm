module;
#include <string_view>
#include <utility>
#include <string>
#include <vector>
#include <thread>
#include <cmath>
#include <samplerate.h>
#include <sndfile.h>
#include "libassert/assert.hpp"
#include "util/log_macros.hpp"

export module playnote.audio_thread;

import playnote.stx.types;
import playnote.sys.window;
import playnote.sys.audio;
import playnote.sys.os;
import playnote.globals;
import playnote.bms;

namespace playnote {

using stx::usize;
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
export void audio_thread(int argc, char* argv[]) {
	sys::set_thread_name("audio");

	auto& window = locator.get<sys::Window>();
	auto [audio, audio_stub] = locator.provide<sys::Audio>(argc, argv);
	auto bms = BMS("songs/Ling Child/02_hyper.bme");
	while (!window.is_closing())
		std::this_thread::yield();
}

}
