#ifndef PLAYNOTE_PLAYNOTE_H
#define PLAYNOTE_PLAYNOTE_H

#include <expected>

class Playnote {
public:
	Playnote();
	auto run() -> std::expected<void, int>;
};

#endif
