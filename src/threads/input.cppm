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

export void input(threads::Broadcaster& broadcaster, dev::GLFW& glfw, dev::Window& window)
try {
	dev::name_current_thread("input");
	broadcaster.register_as_endpoint();
	broadcaster.wait_for_others(3);

	while (!window.is_closing()) {
		glfw.poll();
		FRAME_MARK_NAMED("input");
		yield();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
