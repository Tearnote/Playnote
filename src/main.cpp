import playnote.util.logger;
import playnote.util.math;
import playnote.sys.window;
import playnote.sys.os;

#include <exception>
#include <chrono>
#include <print>
#include "util/log_macros.hpp"
#include "config.hpp"

// Can't namespace main()
using namespace playnote;
using namespace std::chrono_literals; // Temporarily needed
using util::uvec2;
using util::s_logger;
using sys::s_window;

auto main(int, char*[]) -> int
try {
#if BUILD_TYPE == BUILD_DEBUG
	sys::create_console();
#endif
	auto logger = s_logger.provide();
	sys::set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto window = s_window.provide(AppTitle, uvec2(640, 480));

	while (!s_window->is_closing())
		s_window->poll();

	return EXIT_SUCCESS;
}
catch (std::exception const& e) {
	if (s_logger)
		L_CRIT("Uncaught exception: {}", e.what());
	else
		std::print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
