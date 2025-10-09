/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.cpp:
Implementation file for threads/audio.hpp.
*/

#include "threads/input.hpp"

#include "preamble.hpp"
#include "logger.hpp"
#include "dev/controller.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "threads/input_shouts.hpp"
#include "threads/tools.hpp"

namespace playnote::threads {

static void run_input(Tools& tools, dev::Window& window)
{
	auto& glfw = window.get_glfw();
	window.register_key_callback([&](dev::Window::KeyCode keycode, bool state) {
		tools.broadcaster.shout(KeyInput{
			.timestamp = glfw.get_time(),
			.code = keycode,
			.state = state,
		});
	});
	window.register_file_drop_callback([&](span<char const* const> paths) {
		auto event = FileDrop{};
		copy(paths, back_inserter(event.paths));
		tools.broadcaster.shout(move(event));
	});
	auto con_dispatcher = dev::ControllerDispatcher{glfw};
	while (!window.is_closing()) {
		glfw.poll();
		con_dispatcher.poll([&](auto event) {
			visit([&](auto&& e){ tools.broadcaster.shout(move(e)); }, event);
		});
		yield();
	}
}

void input(Tools& tools, dev::Window& window)
try {
	dev::name_current_thread("input");
	tools.broadcaster.register_as_endpoint();
	tools.barriers.startup.arrive_and_wait();
	run_input(tools, window);
	tools.barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	tools.barriers.shutdown.arrive_and_wait();
}

}
