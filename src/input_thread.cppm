module;
#include <thread>
#include "tracy/Tracy.hpp"

export module playnote.input_thread;

import playnote.sys.window;
import playnote.sys.os;
import playnote.globals;

namespace playnote {

// Handle the tasks of the input thread, which is spinning on the OS message queue as much possible
// without saturating the CPU.
// This function needs to be run on the process's initial thread.
export void input_thread()
{
	sys::set_thread_name("input");
	auto& glfw = locator.get<sys::GLFW>();
	auto& window = locator.get<sys::Window>();

	while (!window.is_closing()) {
		glfw.poll();
		FrameMarkNamed("input");
		std::this_thread::yield();
	}
}

}
