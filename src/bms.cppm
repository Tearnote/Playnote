/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms.cppm:
BMS loading and parsing facilities.
*/

module;
#include <string_view>

export module playnote.bms;

import playnote.util.file;

namespace playnote {

export class BMS {
public:
	explicit BMS(std::string_view path);
};

BMS::BMS(std::string_view path)
{
	auto bms_file = util::read_file(path);
}

}
