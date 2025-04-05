import playnote.sys.window;
import playnote.sys.os;

#include <exception>
#include <print>
#include "util/logger.hpp"
#include "util/math.hpp"
#include "config.hpp"

// Can't namespace main()
using namespace playnote;
using namespace std::chrono_literals; // Temporarily needed

auto main(int, char*[]) -> int
try {
#if BUILD_TYPE == BUILD_DEBUG
	sys::create_console();
#endif
	auto logger = s_logger.provide();
	sys::set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto window = sys::s_window.provide(AppTitle, uvec2(640, 480));

	while (!sys::s_window->is_closing())
		sys::s_window->poll();

	return EXIT_SUCCESS;
}
catch (std::exception const& e) {
	if (s_logger)
		L_CRIT("Uncaught exception: {}", e.what());
	else
		std::print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
