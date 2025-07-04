/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

main.cpp:
Entry point. Initializes basic facilities, spawns threads.
*/

#include "macros/logger.hpp"
#include "macros/assert.hpp"

import playnote.preamble;
import playnote.config;
import playnote.logger;
import playnote.lib.debug;
import playnote.dev.window;
import playnote.dev.os;
import playnote.threads.broadcaster;
import playnote.threads.render;
import playnote.threads.audio;
import playnote.threads.input;

using namespace playnote; // Can't namespace main()

auto parse_arguments(int argc, char** argv) -> fs::path
{
	ASSERT(argc == 2, "Expected one argument: song path");
	return fs::path{argv[1]};
}

auto run(fs::path const& song_request) -> int
{
	auto const scheduler_period = dev::SchedulerPeriod{1ms};
	auto glfw = dev::GLFW{};
	auto window = dev::Window{glfw, AppTitle, {1280, 720}};

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	auto broadcaster = threads::Broadcaster{};
	auto barriers = threads::Barriers<3>{};
	auto audio_thread_stub = jthread{threads::audio, ref(broadcaster), ref(barriers), ref(window)};
	auto render_thread_stub = jthread{threads::render, ref(broadcaster), ref(barriers), ref(window)};
	threads::input(broadcaster, barriers, window, song_request);

	return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
try {
	lib::dbg::set_assert_handler();
	if constexpr (BuildType == Build::Debug) lib::dbg::attach_console();
	auto logger_stub = globals::logger.provide(LogfilePath, LogLevelGlobal);
	auto song_request = parse_arguments(argc, argv);
	INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run(song_request);
}
catch (exception const& e) {
	// Handle any exception that happened outside of threads::input()
	if (globals::logger)
		CRIT("Uncaught exception: {}", e.what());
	else
		print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
