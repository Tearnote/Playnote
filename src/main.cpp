#include "util/logger.hpp"
#include "sys/os.hpp"
#include "config.hpp"

class App:
	Logger
{
public:
	App()
	{
		L_INFO("{} {}.{}.{} initializing", AppTitle, AppVersion[0], AppVersion[1], AppVersion[2]);
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
	auto app = App();
	return app.run();
}
