import playnote.stx.math;
import playnote.util.logger;
import playnote.sys.window;
import playnote.sys.gpu;
import playnote.sys.os;

#include <exception>
#include <chrono>
#include <print>
#include "util/log_macros.hpp"
#include "config.hpp"

using namespace playnote; // Can't namespace main()
using namespace std::chrono_literals;
using stx::uvec2;
using util::s_logger;
using sys::s_window;
using sys::s_glfw;
using sys::s_gpu;

auto run() -> int
try {
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto glfw = s_glfw.provide();
	auto window = s_window.provide(AppTitle, uvec2{640, 480});
	auto gpu = s_gpu.provide();

	while (!s_window->is_closing()) {
		s_glfw->poll();
		s_gpu->next_frame();
	}

	return EXIT_SUCCESS;
}
catch (std::exception const& e) {
	// Logger guaranteed to exist here
	L_CRIT("Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}

auto main(int, char*[]) -> int
try {
#if BUILD_TYPE == BUILD_DEBUG
	sys::create_console();
#endif
	auto logger = s_logger.provide();
	sys::set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run();
}
catch (std::exception const& e) {
	// Handle any exception that happened outside of run() just in case
	if (s_logger)
		L_CRIT("Uncaught exception: {}", e.what());
	else
		std::print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
