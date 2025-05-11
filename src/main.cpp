#include <exception>
#include <thread>
#include <chrono>
#include <print>
#include "util/log_macros.hpp"
#include "config.hpp"

import playnote.stx.math;
import playnote.util.logger;
import playnote.sys.window;
import playnote.sys.audio;
import playnote.sys.gpu;
import playnote.sys.os;
import playnote.gfx.renderer;
import playnote.render_thread;
import playnote.audio_thread;
import playnote.input_thread;
import playnote.globals;

using namespace playnote; // Can't namespace main()
using namespace std::chrono_literals;
using stx::uvec2;

auto run(int argc, char* argv[]) -> int
try {
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto [glfw, glfw_stub] = locator.provide<sys::GLFW>();
	auto [window, window_stub] = locator.provide<sys::Window>(glfw, AppTitle, uvec2{640, 480});
	auto [gpu, gpu_stub] = locator.provide<sys::GPU>(window);

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	auto audio_thread_stub = std::thread{audio_thread, argc, argv};
	auto render_thread_stub = std::thread{render_thread};
	input_thread();
	render_thread_stub.join();
	audio_thread_stub.join();

	return EXIT_SUCCESS;
}
catch (std::exception const& e) {
	// Logger guaranteed to exist here
	L_CRIT("Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}

auto main(int argc, char* argv[]) -> int
try {
#if BUILD_TYPE == BUILD_DEBUG
	sys::create_console();
#endif
	auto [logger, logger_stub] = locator.provide<util::Logger>();
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run(argc, argv);
}
catch (std::exception const& e) {
	// Handle any exception that happened outside of run() just in case
	if (locator.exists<util::Logger>())
		L_CRIT("Uncaught exception: {}", e.what());
	else
		std::print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
