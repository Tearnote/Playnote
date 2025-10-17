/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.cpp:
Implementation file for threads/audio.hpp.
*/

#include "input.hpp"

#include "preamble.hpp"
#include "utils/logger.hpp"
#include "lib/os.hpp"
#include "dev/controller.hpp"
#include "dev/window.hpp"

namespace playnote {

static void run_input(Broadcaster& broadcaster, dev::Window& window)
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
		broadcaster.shout(move(event));
	});
	auto con_dispatcher = dev::ControllerDispatcher{};

	while (!window.is_closing()) {
		// Handle queue changes
		broadcaster.receive_all<RegisterInputQueue>([&](auto const& q) {
			input_queues.emplace_back(q.queue.lock());
		});
		broadcaster.receive_all<UnregisterInputQueue>([&](auto const& q) {
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

void input_thread(Broadcaster& broadcaster, Barriers<2>& barriers, dev::Window& window)
try {
	lib::os::name_current_thread("input");
	broadcaster.register_as_endpoint();
	broadcaster.subscribe<RegisterInputQueue>();
	broadcaster.subscribe<UnregisterInputQueue>();
	barriers.startup.arrive_and_wait();
	run_input(broadcaster, window);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	CRIT("Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
