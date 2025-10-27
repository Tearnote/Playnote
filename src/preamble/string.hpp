/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <string_view> // IWYU pragma: export
#include <filesystem>
#include <string> // IWYU pragma: export
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/rational.hpp>
#include "quill/bundled/fmt/format.h"
#include "quill/DeferredFormatCodec.h"
#include "preamble/algorithm.hpp"
#include "preamble/math_ext.hpp"
#include "preamble/concepts.hpp"
#include "preamble/types.hpp"
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
using boost::iequals;
using boost::trim;
using boost::trim_copy;
using boost::replace_all;
using boost::to_upper;
using boost::to_lower;
using boost::to_lower_copy;
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

template<playnote::isize Dim, typename T>
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
template<playnote::isize Dim, typename T>
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
