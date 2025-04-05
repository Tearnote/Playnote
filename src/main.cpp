import playnote.sys.os;

#include <exception>
#include <print>
#include "util/logger.hpp"
#include "sys/window.hpp"
#include "config.hpp"

using namespace playnote;

auto main(int, char*[]) -> int
try {
	auto logger = s_logger.provide();
	sys::set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto window = s_window.provide(AppTitle, uvec2(640, 480));

	while (!s_window->isClosing())
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
