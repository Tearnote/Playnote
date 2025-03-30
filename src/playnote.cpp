#include "playnote.hpp"

#include <expected>
#include "util/time.hpp"
#include "sys/os.hpp"
#include "config.hpp"

Playnote::Playnote()
{
	set_thread_name("input");
	L_INFO("{} {}.{}.{} initializing", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
	auto scheduler_period = SchedulerPeriod(1ms);
}

auto Playnote::run() -> std::expected<void, int>
{
	return {};
}
