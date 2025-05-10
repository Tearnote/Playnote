module;
#include <thread>

export module playnote.input_thread;

import playnote.sys.window;
import playnote.globals;

namespace playnote {

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
