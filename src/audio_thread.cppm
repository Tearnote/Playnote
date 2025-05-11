module;

export module playnote.audio_thread;

import playnote.sys.audio;
import playnote.sys.os;
import playnote.globals;

namespace playnote {

export void audio_thread(int argc, char* argv[]) {
	sys::set_thread_name("audio");
	auto [audio, audio_stub] = locator.provide<sys::Audio>(argc, argv);
	audio.run();
}

}
