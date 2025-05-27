/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

main.cpp:
Entry point. Initializes basic facilities, spawns threads.
*/

#include "macros/logger.hpp"

import playnote.preamble;
import playnote.config;
import playnote.logger;
import playnote.lib.debug;
import playnote.sys.window;
import playnote.sys.os;
import playnote.threads.render;
import playnote.threads.audio;
import playnote.threads.input;

using namespace playnote; // Can't namespace main()

auto run() -> int
{
	auto const scheduler_period = sys::SchedulerPeriod{1ms};
	auto glfw = sys::GLFW{};
	auto window = sys::Window{glfw, AppTitle, {640, 480}};

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	auto audio_thread_stub = jthread{threads::audio, ref(window)};
	auto render_thread_stub = jthread{threads::render, ref(window)};
	threads::input(glfw, window);

	return EXIT_SUCCESS;
}

auto main() -> int
try {
	lib::dbg::set_assert_handler();
	if constexpr (BuildType == Build::Debug) lib::dbg::attach_console();
	auto logger_stub = globals::logger.provide(LogfilePath, LogLevelGlobal);
	INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run();
}
catch (exception const& e) {
	// Handle any exception that happened outside of threads::input()
	if (globals::logger)
		CRIT("Uncaught exception: {}", e.what());
	else
		print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
