/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

input_thread.cppm:
Spinning on the OS message queue as much possible without saturating the CPU.
Only the process's main thread can be the input thread.
*/

module;
#include <exception>
#include <thread>
#include "tracy/Tracy.hpp"
#include "util/log_macros.hpp"

export module playnote.input_thread;

import playnote.sys.window;
import playnote.sys.os;
import playnote.globals;

namespace playnote {

export void input_thread(sys::GLFW& glfw, sys::Window& window)
try {
	sys::set_thread_name("input");

	while (!window.is_closing()) {
		glfw.poll();
		FrameMarkNamed("input");
		std::this_thread::yield();
	}
}
catch (std::exception const& e) {
	L_CRIT("Uncaught exception: {}", e.what());
	window.request_close();
}

}
