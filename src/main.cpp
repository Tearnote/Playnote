/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
except according to those terms.
*/

#include <cstdlib>
#include <clocale>
#include "preamble.hpp"
#include "utils/broadcaster.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "lib/debug.hpp"
#include "lib/os.hpp"
#include "dev/window.hpp"
#include "dev/os.hpp"
#include "render.hpp"
#include "input.hpp"

using namespace playnote; // Can't namespace main()

auto run() -> int
{
	auto const scheduler_period = dev::SchedulerPeriod{1ms};
	auto glfw_stub = globals::glfw.provide();
	auto window = dev::Window{AppTitle, {1280, 720}};

	// Spawn all threads. Every thread is assumed to eventually finish
	// once window.is_closing() is true
	auto broadcaster = Broadcaster{};
	auto barriers = Barriers<2>{};
	auto render_thread_stub = jthread{render_thread, ref(broadcaster), ref(barriers), ref(window)};
	input_thread(broadcaster, barriers, window);

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
	globals::config->load_from_file();
	if (globals::config->get_entry<bool>("system", "attach_console")) lib::dbg::attach_console();
	auto global_log_level = globals::config->get_entry<string>("logging", "global");
	auto logger_stub = globals::logger.provide(LogfilePath,
		*enum_cast<Logger::Level>(global_log_level).or_else([&] -> optional<Logger::Level> {
			throw runtime_error_fmt("Invalid log level: {}", global_log_level);
		}
	));
	INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	lib::os::check_mimalloc();
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
