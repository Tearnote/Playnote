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
#include "threads/broadcaster.hpp"

namespace playnote::threads {

static void run_input(Broadcaster& broadcaster, dev::Window& window, ChartRequest const& request)
{
	auto& glfw = window.get_glfw();
	window.register_key_callback([&](dev::Window::KeyCode keycode, bool state) {
		broadcaster.shout(KeyInput{
			.timestamp = glfw.get_time(),
			.code = keycode,
			.state = state,
		});
	});
	auto con_dispatcher = dev::ControllerDispatcher{glfw};
	broadcaster.shout(request);
	while (!window.is_closing()) {
		glfw.poll();
		con_dispatcher.poll([&](auto event) {
			visit([&](auto&& e){ broadcaster.shout(move(e)); }, event);
		});
		yield();
	}
}

void input(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window, ChartRequest const& request)
try {
	dev::name_current_thread("input");
	broadcaster.register_as_endpoint();
	barriers.startup.arrive_and_wait();
	run_input(broadcaster, window, request);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
