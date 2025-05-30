/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports for string and text IO.
*/

module;
#include <string_view>
#include <filesystem>
#include <ranges>
#include <string>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include "quill/bundled/fmt/format.h"
#include "quill/DeferredFormatCodec.h"

export module playnote.preamble:string;

import :algorithm;
import :math_ext;
import :concepts;
import :os;

namespace playnote {

export using std::string;
export using std::string_view;
export using std::literals::operator ""s;
export using std::literals::operator ""sv;
export using fmtquill::format;
export using fmtquill::print;
export using boost::trim;
export using boost::trim_copy;
export using boost::replace_all;
export using boost::to_upper;
export using boost::to_lower;
export using boost::lexical_cast;
export using boost::bad_lexical_cast;

// Return a prefix of a string until a character that matches a predicate, not including
// that character.
export template<callable<bool(char)> Func>
auto substr_until(string_view text, Func&& pred) noexcept -> string_view
{
	auto found = find_if(text, pred);
	return string_view{text.begin(), found};
}

}

export template<>
struct fmtquill::formatter<playnote::fs::path>: formatter<std::string_view> {
	auto format(playnote::fs::path const& c, format_context& ctx) const -> format_context::iterator
	{
		return formatter<std::string_view>::format(c.c_str(), ctx);
	}
};
export template<>
struct quill::Codec<playnote::fs::path>: DeferredFormatCodec<playnote::fs::path> {};

export template<>
struct fmtquill::formatter<std::filesystem::directory_entry>: formatter<std::string_view> {
	auto format(std::filesystem::directory_entry const& d, format_context& ctx) const -> format_context::iterator
	{
		return formatter<playnote::fs::path>{}.format(d.path(), ctx);
	}
};
export template<>
struct quill::Codec<std::filesystem::directory_entry>: DeferredFormatCodec<std::filesystem::directory_entry> {};

export template<playnote::usize Dim, typename T>
struct fmtquill::formatter<playnote::vec<Dim, T>> {
	template<typename Ctx>
	constexpr auto parse(Ctx& ctx) { return formatter<T>{}.parse(ctx); }

	template<typename Ctx>
	auto format(playnote::vec<Dim, T> const& v, Ctx& ctx) const -> format_context::iterator
	{
		format_to(ctx.out(), "(");
		format_to(formatter<T>{}.format(v.x(), ctx), ", ");
		if constexpr(Dim == 2) {
			format_to(formatter<T>{}.format(v.y(), ctx), "");
		} else if constexpr(Dim == 3) {
			format_to(formatter<T>{}.format(v.y(), ctx), ", ");
			format_to(formatter<T>{}.format(v.z(), ctx), "");
		} else if constexpr(Dim == 4) {
			format_to(formatter<T>{}.format(v.y(), ctx), ", ");
			format_to(formatter<T>{}.format(v.z(), ctx), ", ");
			format_to(formatter<T>{}.format(v.w(), ctx), "");
		}
		return format_to(ctx.out(), ")");
	}
};
export template<playnote::usize Dim, typename T>
struct quill::Codec<playnote::vec<Dim, T>>: DeferredFormatCodec<playnote::vec<Dim, T>> {};
