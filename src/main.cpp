#include <exception>
#include <print>
#include "util/logger.hpp"
#include "sys/window.hpp"
#include "sys/os.hpp"
#include "config.hpp"

auto main(int, char*[]) -> int
try {
	auto logger = s_logger.provide();
	set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	auto scheduler_period = SchedulerPeriod(1ms);
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
