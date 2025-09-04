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
	auto const actual_args = argc - 1;
	constexpr auto usage_str = "Usage:\nplaynote <BMS file path>\nor\nplaynote <archive path> <BMS file name>";
	if (actual_args == 0) {
		dev::syserror("{}", usage_str);
		exit(EXIT_SUCCESS);
	}
	if (actual_args > 2) {
		dev::syserror("Invalid number of arguments: expected 1 or 2, received {}\n\n{}", actual_args, usage_str);
		exit(EXIT_FAILURE);
	}

	if (actual_args == 2) {
		return threads::ChartRequest{
			.domain = fs::path{argv[1]},
			.filename = argv[2],
		};
	}
	// 1 arg, split into location and filename
	auto const path = fs::path{argv[1]};
	return threads::ChartRequest{
		.domain = path.parent_path(),
		.filename = path.filename().string(),
	};
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

#ifdef TARGET_LINUX
auto main(int argc, char** argv) -> int
#elifdef TARGET_WINDOWS
auto WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int
#endif
try {
#ifdef TARGET_WINDOWS
	auto const argc = __argc;
	auto** argv = __argv;
#endif
	lib::dbg::set_assert_handler();
	auto config_stub = globals::config.provide();
	if (globals::config->get_entry<bool>("system", "attach_console")) lib::dbg::attach_console();
	auto logger_stub = globals::logger.provide(LogfilePath,
		*enum_cast<Logger::Level>(globals::config->get_entry<string>("logging", "global")));
	globals::config->load_from_file();
	auto const song_request = parse_arguments(argc, argv);
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
