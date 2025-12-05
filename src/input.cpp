/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
except according to those terms.
*/

#include "input.hpp"

#include "preamble.hpp"
#include "utils/broadcaster.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "lib/os.hpp"
#include "dev/controller.hpp"
#include "dev/window.hpp"

namespace playnote {

static void run_input(Broadcaster& broadcaster, dev::Window& window, Logger::Category cat)
{
	// Register input handlers
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
		TRACE_AS(cat, "{} path(s) dropped:", event.paths.size());
		for (auto const& path: event.paths) TRACE_AS(cat, "  {}", path);
		broadcaster.shout(move(event));
	});
	auto con_dispatcher = dev::ControllerDispatcher{cat};

	while (!window.is_closing()) {
		// Handle queue changes
		for (auto q: broadcaster.receive_all<RegisterInputQueue>()) {
			input_queues.emplace_back(q.queue.lock());
			TRACE_AS(cat, "Registered input queue");
		}
		for (auto q: broadcaster.receive_all<UnregisterInputQueue>()) {
			auto queue = q.queue.lock();
			auto it = find(input_queues, queue);
			if (it != input_queues.end()) {
				input_queues.erase(it);
				TRACE_AS(cat, "Unregistered input queue");
			} else {
				WARN_AS(cat, "Attempted to unregister input queue that was not registered");
			}
		}

		// Poll and handle input events
		globals::glfw->poll();
		for (auto event: con_dispatcher.poll()) {
			visit([&](auto&& e) {
				for (auto& queue: input_queues) {
					queue->enqueue(move(e));
				}
			}, event);
		}
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
	auto cat = globals::logger->create_category("Input",
		*enum_cast<Logger::Level>(globals::config->get_entry<string>("logging", "input")));
	run_input(broadcaster, window, cat);
	barriers.shutdown.arrive_and_wait();
}
catch (exception const& e) {
	auto cat = globals::logger->create_category("Input");
	CRIT_AS(cat, "Uncaught exception: {}", e.what());
	window.request_close();
	barriers.shutdown.arrive_and_wait();
}

}
