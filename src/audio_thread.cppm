module;
#include <utility>
#include <memory>
#include <thread>
#include <cmath>
#include <samplerate.h>
#include <sndfile.h>
#include "util/log_macros.hpp"

export module playnote.audio_thread;

import playnote.stx.types;
import playnote.sys.window;
import playnote.sys.audio;
import playnote.sys.os;
import playnote.globals;

namespace playnote {

using stx::usize;

auto load_test_file() -> void
{
	auto file_info = SF_INFO{};
	auto* file = sf_open("assets/test.ogg", SFM_READ, &file_info);
	if (!file) {
		L_ERROR("Failed to open assets/test.ogg: {}", sf_strerror(nullptr));
		return;
	}
	auto samples_raw = std::make_unique<float[]>(file_info.frames * file_info.channels);
	sf_readf_float(file, samples_raw.get(), file_info.frames);
	sf_close(file);

	auto samples = [&]() {
		if (file_info.samplerate == 48000) {
			return std::move(samples_raw);
		} else {
			auto ratio = 48000.0 / static_cast<double>(file_info.samplerate);
			auto output_frame_count = static_cast<long>(std::ceil(file_info.frames * ratio));
			output_frame_count = output_frame_count + 8 - (output_frame_count % 8);
			auto samples = std::make_unique<float[]>(output_frame_count * file_info.channels);

			auto src_data = SRC_DATA{
				.data_in = samples_raw.get(),
				.data_out = samples.get(),
				.input_frames = file_info.frames,
				.output_frames = output_frame_count,
				.src_ratio = ratio,
			};
			auto ret = src_simple(&src_data, SRC_SINC_BEST_QUALITY, file_info.channels);
			if (ret != 0) {
				L_ERROR("Failed to resample audio: {}", src_strerror(ret));
			}
			return std::move(samples);
		}
	};
}

export void audio_thread(int argc, char* argv[]) {
	sys::set_thread_name("audio");

	auto& window = locator.get<sys::Window>();
	auto [audio, audio_stub] = locator.provide<sys::Audio>(argc, argv);
	load_test_file();
	while (!window.is_closing())
		std::this_thread::yield();
}

}
