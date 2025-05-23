/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports for string and text IO.
*/

module;
#include <filesystem>
#include <string>
#include "quill/bundled/fmt/format.h"

export module playnote.preamble:string;

import :os;

namespace playnote {

export using std::string;
export using fmtquill::format;
export using fmtquill::print;

}

export template<>
struct fmtquill::formatter<playnote::fs::path>: formatter<std::string_view> {
	auto format(playnote::fs::path const& c, format_context& ctx) const -> format_context::iterator
	{
		return formatter<std::string_view>::format(c.c_str(), ctx);
	}
};
