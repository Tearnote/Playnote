/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports for string and text IO.
*/

module;
#include <string_view>
#include <filesystem>
#include <string>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/lexical_cast.hpp>
#include "quill/bundled/fmt/format.h"

export module playnote.preamble:string;

import :os;

namespace playnote {

export using std::string;
export using std::string_view;
export using fmtquill::format;
export using fmtquill::print;
export using boost::trim;
export using boost::replace_all;
export using boost::to_upper;
export using boost::to_lower;
export using boost::lexical_cast;
export using boost::bad_lexical_cast;

}

export template<>
struct fmtquill::formatter<playnote::fs::path>: formatter<std::string_view> {
	auto format(playnote::fs::path const& c, format_context& ctx) const -> format_context::iterator
	{
		return formatter<std::string_view>::format(c.c_str(), ctx);
	}
};
