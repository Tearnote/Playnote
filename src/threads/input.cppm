/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.cppm:
Spinning on the OS message queue as much possible without saturating the CPU.
Only the process's main thread can be the input thread.
*/

module;
#include "tracy/Tracy.hpp"
#include "macros/logger.hpp"

export module playnote.threads.input;

import playnote.preamble;
import playnote.logger;
import playnote.sys.window;
import playnote.sys.os;

namespace playnote::threads {

export void input(sys::GLFW& glfw, sys::Window& window)
try {
	sys::set_thread_name("input");

	while (!window.is_closing()) {
		glfw.poll();
		FrameMarkNamed("input");
		yield();
	}
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
