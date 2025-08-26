/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.hpp:
Imports for string and text IO.
*/

#pragma once
#include <string_view>
#include <filesystem>
#include <string>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/rational.hpp>
#include "quill/bundled/fmt/format.h"
#include "quill/DeferredFormatCodec.h"
#include "preamble/math_ext.hpp"
#include "preamble/concepts.hpp"
#include "preamble/math.hpp"
#include "preamble/os.hpp"

namespace playnote {

using std::string;
using std::string_view;
using std::literals::operator""s;
using std::literals::operator""sv;
using fmtquill::format_string;
using fmtquill::format;
using fmtquill::print;
using boost::trim;
using boost::trim_copy;
using boost::replace_all;
using boost::to_upper;
using boost::to_lower;
using boost::lexical_cast;
using boost::bad_lexical_cast;

// Return a prefix of a string until a character that matches a predicate, not including
// that character.
template<callable<bool(char)> Func>
auto substr_until(string_view text, Func&& pred) -> string_view
{
	auto found = find_if(text, pred);
	return string_view{text.begin(), found};
}

// Run a function for each line in a string.
template<callable<void(string_view)> Func>
void for_each_line(string_view text, Func&& func)
{
	auto pos = 0zu;
	while (pos < text.size()) {
		auto const next = text.find('\n', pos);
		if (next == string_view::npos) {
			func(text.substr(pos));
			break;
		}
		func(text.substr(pos, next - pos));
		pos = next + 1;
	}
}

}

template<>
struct fmtquill::formatter<playnote::fs::path>: formatter<std::string_view> {
	auto format(playnote::fs::path const& c, format_context& ctx) const -> format_context::iterator
	{
		return formatter<std::string_view>::format(c.string(), ctx);
	}
};
template<>
struct quill::Codec<playnote::fs::path>: DeferredFormatCodec<playnote::fs::path> {};

template<>
struct fmtquill::formatter<std::filesystem::directory_entry>: formatter<std::string_view> {
	auto format(std::filesystem::directory_entry const& d, format_context& ctx) const -> format_context::iterator
	{
		return formatter<playnote::fs::path>{}.format(d.path(), ctx);
	}
};
template<>
struct quill::Codec<std::filesystem::directory_entry>: DeferredFormatCodec<std::filesystem::directory_entry> {};

template<playnote::usize Dim, typename T>
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
template<playnote::usize Dim, typename T>
struct quill::Codec<playnote::vec<Dim, T>>: DeferredFormatCodec<playnote::vec<Dim, T>> {};

template<typename T>
struct fmtquill::formatter<boost::rational<T>>: formatter<int> {
	auto format(boost::rational<T> const& r, format_context& ctx) const -> format_context::iterator
	{
		auto const fractional_part = playnote::fract(r);
		format_to(formatter<T>{}.format(playnote::trunc(r), ctx), " ");
		format_to(formatter<T>{}.format(fractional_part.numerator(), ctx), "/");
		return format_to(formatter<T>{}.format(fractional_part.denominator(), ctx), "");
	}
};
template<typename T>
struct quill::Codec<boost::rational<T>>: DeferredFormatCodec<boost::rational<T>> {};
