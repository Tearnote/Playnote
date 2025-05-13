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
	auto [bms_raw_mapping, bms_raw_contents] = util::read_file(path);
}

}
