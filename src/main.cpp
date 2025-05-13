#include <exception>
#include <thread>
#include <chrono>
#include <print>
#include "util/log_macros.hpp"
#include "config.hpp"

import playnote.stx.math;
import playnote.util.logger;
import playnote.sys.window;
import playnote.sys.os;
import playnote.render_thread;
import playnote.audio_thread;
import playnote.input_thread;
import playnote.globals;

using namespace playnote; // Can't namespace main()
using namespace std::chrono_literals;
using stx::uvec2;

auto run(int argc, char* argv[]) -> int
{
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto glfw = sys::GLFW{};
	auto window = sys::Window{glfw, AppTitle, uvec2{640, 480}};

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	{
		auto audio_thread_stub = std::jthread{audio_thread, std::ref(window), argc, argv};
		auto render_thread_stub = std::jthread{render_thread, std::ref(window)};
		input_thread(glfw, window);
	}

	return EXIT_SUCCESS;
}

auto main(int argc, char* argv[]) -> int
try {
#if BUILD_TYPE == BUILD_DEBUG
	sys::create_console();
#endif
	g_logger = util::Logger{};
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run(argc, argv);
}
catch (std::exception const& e) {
	// Handle any exception that happened outside of input_thread()
	if (g_logger)
		L_CRIT("Uncaught exception: {}", e.what());
	else
		std::print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
