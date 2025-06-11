/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.cppm:
Spinning on the OS message queue as much possible without saturating the CPU.
Only the process's main thread can be the input thread.
*/

module;
#include "macros/tracing.hpp"
#include "macros/logger.hpp"

export module playnote.threads.input;

import playnote.preamble;
import playnote.logger;
import playnote.dev.window;
import playnote.dev.os;
import playnote.threads.broadcaster;

namespace playnote::threads {

export void input(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window, fs::path const& song_request)
try {
	dev::name_current_thread("input");
	auto& glfw = window.get_glfw();
	broadcaster.register_as_endpoint();
	barriers.startup.arrive_and_wait();
	broadcaster.shout(song_request);

	while (!window.is_closing()) {
		glfw.poll();
		FRAME_MARK_NAMED("input");
		yield();
	}

	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
