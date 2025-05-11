module;
#include <thread>

export module playnote.input_thread;

import playnote.sys.window;
import playnote.globals;

namespace playnote {

// Handle the tasks of the input thread, which is spinning on the OS message queue as much possible
// without saturating the CPU.
// This function needs to be run on the process's initial thread.
export void input_thread()
{
	auto& glfw = locator.get<sys::GLFW>();
	auto& window = locator.get<sys::Window>();

	while (!window.is_closing()) {
		glfw.poll();
		std::this_thread::yield();
	}
}

}
