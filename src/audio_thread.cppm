module;
#include <thread>
#include <sndfile.h>
#include "util/log_macros.hpp"

export module playnote.audio_thread;

import playnote.sys.window;
import playnote.sys.audio;
import playnote.sys.os;
import playnote.globals;

namespace playnote {

auto load_test_file() -> void
{
	auto file_info = SF_INFO{};
	auto* file = sf_open("assets/test.ogg", SFM_READ, &file_info);
	if (!file) {
		L_ERROR("Failed to open assets/test.ogg: {}", sf_strerror(nullptr));
		return;
	}
	auto samples = std::make_unique<float[]>(file_info.frames * file_info.channels);
	sf_readf_float(file, samples.get(), file_info.frames);
	sf_close(file);
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
