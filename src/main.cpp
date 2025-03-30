#include <exception>
#include <expected>
#include <cstdio>
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

	auto run() -> std::expected<void, int>
	{
		return {};
	}
};

auto main(int argc, char* argv[]) -> int try
{
	set_thread_name("input");
	auto logger = Logger::Provider();
	auto playnote = Playnote();
	return playnote.run().error_or(EXIT_SUCCESS);
}
catch (std::exception& e)
{
	std::fprintf(stderr, "Uncaught exception: %s", e.what());
}
