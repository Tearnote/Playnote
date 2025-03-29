#include <utility>
#include "config.hpp"

#include "util/logger.hpp"

class App:
	Logger
{
public:
	App() {
		L_INFO("Hello World!");
	}

	auto run() -> int {
		return 0;
	}
};

auto main(int argc, char* argv[]) -> int
{
	auto logger = Logger::Provider();
	auto app = App();
	return app.run();
}
