#pragma once

#include <expected>

class Playnote {
public:
	Playnote();
	auto run() -> std::expected<void, int>;
};
