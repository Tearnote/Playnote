/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports for string and text IO.
*/

module;
#include <filesystem>
#include <string>
#include <format>
#include <print>

export module playnote.preamble:string;

namespace playnote {

export using std::string;
export using std::format;
export using std::print;

}

// Simple fs::path formatter
#ifndef __cpp_lib_format_path
export template<typename CharT>
struct std::formatter<std::filesystem::path, CharT> {
	constexpr auto parse(std::format_parse_context& ctx) {
		auto it = ctx.begin();
		while (it != ctx.end() && *it != '}')
			it += 1;
		return it;
	}

	auto format(const std::filesystem::path& path, std::format_context& ctx) const {
		return std::format_to(ctx.out(), "\"{}\"", path.c_str());
	}

};
#endif
