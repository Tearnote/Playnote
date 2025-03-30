#include <exception>
#include <print>
#include "util/logger.hpp"
#include "playnote.hpp"

auto main(int argc, char* argv[]) -> int try
{
	auto logger = Logger::Provider();
	auto playnote = Playnote();
	return playnote.run().error_or(EXIT_SUCCESS);
}
catch (std::exception& e)
{
	// In case anything throws before the logger is ready
	std::print(stderr, "Uncaught exception: {}", e.what());
}
