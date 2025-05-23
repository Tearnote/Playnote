/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

main.cpp:
Entry point. Initializes basic facilities like logging and shared subsystems, then spawns threads.
*/

#include <print>
#include "util/log_macros.hpp"
#include "config.hpp"

import playnote.preamble;
import playnote.util.charset;
import playnote.util.logger;
import playnote.sys.window;
import playnote.sys.os;
import playnote.render_thread;
import playnote.audio_thread;
import playnote.input_thread;

using namespace playnote; // Can't namespace main()

auto run() -> int
{
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto glfw = sys::GLFW{};
	auto window = sys::Window{glfw, AppTitle, {640, 480}};

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	auto audio_thread_stub = jthread{audio_thread, ref(window)};
	auto render_thread_stub = jthread{render_thread, ref(window)};
	input_thread(glfw, window);

	return EXIT_SUCCESS;
}

auto main() -> int
try {
#if BUILD_TYPE == BUILD_DEBUG
	sys::create_console();
#endif
	auto logger = util::Logger{LogfilePath, LoggingLevel};
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	util::init_global_formatters();
	return run();
}
catch (exception const& e) {
	// Handle any exception that happened outside of input_thread()
	if (util::log_category_global)
		L_CRIT("Uncaught exception: {}", e.what());
	else
		print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
