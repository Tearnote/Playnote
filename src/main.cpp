/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

main.cpp:
Entry point. Initializes basic facilities, spawns threads.
*/

#include "preamble.hpp"
#include "assert.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "lib/debug.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "threads/input_shouts.hpp"
#include "threads/broadcaster.hpp"
#include "threads/render.hpp"
#include "threads/audio.hpp"
#include "threads/input.hpp"

using namespace playnote; // Can't namespace main()

auto parse_arguments(int argc, char** argv) -> threads::ChartRequest
{
	constexpr auto usage_str = "Usage: playnote <BMS file path>";
	if (argc <= 1) {
		dev::syserror("{}", usage_str);
		exit(EXIT_SUCCESS);
	}
	if (argc > 2) {
		dev::syserror("Invalid number of arguments: expected 1, received {}\n{}", argc - 1, usage_str);
		exit(EXIT_FAILURE);
	}
	return {fs::path{argv[1]}};
}

auto run(threads::ChartRequest const& song_request) -> int
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

#ifndef _WIN32
auto main(int argc, char** argv) -> int
#else
auto WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int
#endif
try {
#ifdef _WIN32
	auto const argc = __argc;
	auto** argv = __argv;
#endif
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
		dev::syserror("Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
