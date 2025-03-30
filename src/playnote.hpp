#pragma once

#include <expected>
#include "util/logger.hpp"

class Playnote:
	Logger
{
public:
	Playnote();

	auto run() -> std::expected<void, int>;
};
