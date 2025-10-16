/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.cpp:
Implementation file for threads/audio.hpp.
*/

#include "threads/input.hpp"

#include "preamble.hpp"
#include "utils/logger.hpp"
#include "dev/controller.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "threads/tools.hpp"

namespace playnote::threads {

static void run_input(Tools& tools, dev::Window& window)
{
	auto input_queues = vector<shared_ptr<spsc_queue<UserInput>>>{};
	window.register_key_callback([&](dev::Window::KeyCode keycode, bool state) {
		for (auto& queue: input_queues) {
			queue->enqueue(KeyInput{
				.timestamp = globals::glfw->get_time(),
				.code = keycode,
				.state = state,
			});
		}
	});
	window.register_file_drop_callback([&](span<char const* const> paths) {
		auto event = FileDrop{};
		copy(paths, back_inserter(event.paths));
		tools.broadcaster.shout(move(event));
	});
	auto con_dispatcher = dev::ControllerDispatcher{};

	while (!window.is_closing()) {
		// Handle queue changes
		tools.broadcaster.receive_all<RegisterInputQueue>([&](auto const& q) {
			input_queues.emplace_back(q.queue.lock());
		});
		tools.broadcaster.receive_all<UnregisterInputQueue>([&](auto const& q) {
			auto queue = q.queue.lock();
			auto it = find(input_queues, queue);
			if (it != input_queues.end())
				input_queues.erase(it);
		});

		// Poll and handle input events
		globals::glfw->poll();
		con_dispatcher.poll([&](auto event) {
			visit([&](auto&& e) {
				for (auto& queue: input_queues) {
					queue->enqueue(move(e));
				}
			}, event);
		});
		yield();
	}
}

void input(Tools& tools, dev::Window& window)
try {
	dev::name_current_thread("input");
	tools.broadcaster.register_as_endpoint();
	tools.broadcaster.subscribe<RegisterInputQueue>();
	tools.broadcaster.subscribe<UnregisterInputQueue>();
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
