#include "util/logger.hpp"
#include "sys/os.hpp"
#include "config.hpp"

class Playnote:
	Logger
{
public:
	Playnote()
	{
		L_INFO("{} {}.{}.{} initializing", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
		auto scheduler_period = SchedulerPeriod(1ms);
	}

	auto run() -> int
	{
		return 0;
	}
};

auto main(int argc, char* argv[]) -> int
{
	set_thread_name("input");
	auto logger = Logger::Provider();
	auto playnote = Playnote();
	return playnote.run();
}
