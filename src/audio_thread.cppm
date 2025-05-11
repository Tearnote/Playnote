module;
#include <thread>

export module playnote.audio_thread;

import playnote.sys.window;
import playnote.sys.audio;
import playnote.sys.os;
import playnote.globals;

namespace playnote {

export void audio_thread(int argc, char* argv[]) {
	sys::set_thread_name("audio");

	auto& window = locator.get<sys::Window>();
	auto [audio, audio_stub] = locator.provide<sys::Audio>(argc, argv);
	while (!window.is_closing())
		std::this_thread::yield();
}

}
