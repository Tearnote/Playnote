/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

main.cpp:
Entry point. Initializes basic facilities, spawns threads.
*/

#include <clocale>
#include "preamble.hpp"
#include "assert.hpp"
#include "config.hpp"
#include "logger.hpp"
#include "lib/debug.hpp"
#include "lib/coro.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "threads/render.hpp"
#include "threads/tools.hpp"
#include "threads/input.hpp"

using namespace playnote; // Can't namespace main()

auto run() -> int
{
	auto const scheduler_period = dev::SchedulerPeriod{1ms};
	auto glfw = dev::GLFW{};
	auto window = dev::Window{glfw, AppTitle, {1280, 720}};

	auto tools = threads::Tools{};
	tools.coro_pool = coro::thread_pool::make_shared();

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	auto render_thread_stub = jthread{threads::render, ref(tools), ref(window)};
	threads::input(tools, window);

	return EXIT_SUCCESS;
}

#ifdef TARGET_LINUX
auto main() -> int
#elifdef TARGET_WINDOWS
auto WinMain(HINSTANCE, HINSTANCE, LPSTR, int) -> int
#endif
try {
	std::setlocale(LC_ALL, "en_US.UTF-8"); //TODO remove after forking libarchive
	lib::dbg::set_assert_handler();
	auto config_stub = globals::config.provide();
	if (globals::config->get_entry<bool>("system", "attach_console")) lib::dbg::attach_console();
	auto logger_stub = globals::logger.provide(LogfilePath,
		*enum_cast<Logger::Level>(globals::config->get_entry<string>("logging", "global")));
	globals::config->load_from_file();
	INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run();
}
catch (exception const& e) {
	// Handle any exception that happened outside of threads::input()
	if (globals::logger)
		CRIT("Uncaught exception: {}", e.what());
	else
		dev::syserror("Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
