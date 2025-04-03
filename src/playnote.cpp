#include "playnote.hpp"

#include <expected>
#include "util/time.hpp"
#include "sys/window.hpp"
#include "sys/os.hpp"
#include "event_loop.hpp"
#include "config.hpp"

Playnote::Playnote()
{
	set_thread_name("input");
	L_INFO("{} {}.{}.{} starting up", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
}

auto Playnote::run() -> std::expected<void, int> try
{
	auto scheduler_period = SchedulerPeriod(1ms);
	auto window = s_window.provide(AppTitle, uvec2(640, 480));
	auto event_loop = EventLoop();
	event_loop.process();
	return {};
}
catch (std::exception &e)
{
	L_CRIT("Uncaught exception: {}", e.what());
	return std::unexpected(EXIT_FAILURE);
}
