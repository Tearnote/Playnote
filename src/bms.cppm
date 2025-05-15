/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms.cppm:
BMS loading and parsing facilities.
*/

module;
#include "util/log_macros.hpp"

export module playnote.bms;

import playnote.util.charset;
import playnote.util.file;
import playnote.globals;

namespace playnote {

export class BMS {
public:
	explicit BMS(std::string_view path);
};

BMS::BMS(std::string_view path)
{
	L_DEBUG("Loading BMS file: \"{}\"", path);
	auto file = util::read_file(path);
	auto encoding = util::detect_text_encoding(file.contents);
	if (encoding.empty()) {
		L_WARN("Failed to detect encoding for \"{}\", assuming Shift_JIS", path);
		encoding = "Shift_JIS";
	}
	auto contents = util::text_to_unicode(file.contents, encoding);
}

}
