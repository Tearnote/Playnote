import playnote.stx.math;
import playnote.util.logger;
import playnote.sys.window;
import playnote.sys.gpu;
import playnote.sys.os;
import playnote.gfx.renderer;
import playnote.globals;

#include <exception>
#include <chrono>
#include <print>
#include "util/log_macros.hpp"
#include "config.hpp"

using namespace playnote; // Can't namespace main()
using namespace std::chrono_literals;
using stx::uvec2;

auto run() -> int
try {
	auto scheduler_period = sys::SchedulerPeriod{1ms};
	auto [glfw, glfw_stub] = locator.provide<sys::GLFW>();
	auto [window, window_stub] = locator.provide<sys::Window>(glfw, AppTitle, uvec2{640, 480});
	auto [gpu, gpu_stub] = locator.provide<sys::GPU>(window);
	auto [renderer, renderer_stub] = locator.provide<gfx::Renderer>(gpu);

	while (!window.is_closing()) {
		glfw.poll();
		renderer.draw();
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
	auto [logger, logger_stub] = locator.provide<util::Logger>();
	sys::set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	return run();
}
catch (std::exception const& e) {
	// Handle any exception that happened outside of run() just in case
	if (locator.exists<util::Logger>())
		L_CRIT("Uncaught exception: {}", e.what());
	else
		std::print(stderr, "Uncaught exception: {}", e.what());
	return EXIT_FAILURE;
}
