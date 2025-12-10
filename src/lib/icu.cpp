/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/icu.hpp"

#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/ucsdet.h>
#include <unicode/uchar.h>
#include <unicode/utext.h>
#include <unicode/ucnv.h>
#include <unicode/utf8.h>
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/logger.hpp"

namespace playnote::lib::icu {

// Throw runtime_error on any ICU error, other than invalid byte during decoding.
static void handle_icu_error(UErrorCode err)
{
	if (err == U_ZERO_ERROR) return;
	if (U_SUCCESS(err)) {
		// Too noisy
		// WARN("ICU warning: {}", u_errorName(err));
		return;
	}
	if (err == U_ILLEGAL_CHAR_FOUND) {
		WARN("Illegal character found in file");
		return;
	}
	throw runtime_error_fmt("ICU error: {}", u_errorName(err));
}

auto detect_encoding(span<byte const> input, initializer_list<string_view> charsets) -> optional<string>
{
	using Detector = unique_resource<UCharsetDetector*, decltype([](auto* d) {
		ucsdet_close(d);
	})>;

	auto err = U_ZERO_ERROR;
	auto const detector = Detector{ASSERT_VAL(ucsdet_open(&err))};
	handle_icu_error(err);
	ucsdet_setText(detector.get(), reinterpret_cast<char const*>(input.data()), input.size(), &err);
	handle_icu_error(err);

	auto* matches_ptr = static_cast<UCharsetMatch const**>(nullptr);
	auto matches_count = 0;
	matches_ptr = ucsdet_detectAll(detector.get(), &matches_count, &err);
	handle_icu_error(err);

	if (!matches_count) return nullopt;
	auto const matches = std::span{matches_ptr, static_cast<size_t>(matches_count)};
	if (empty(charsets)) { // No filtering
		auto* match_name = ucsdet_getName(matches[0], &err);
		handle_icu_error(err);
		return match_name;
	}
	for (auto const& match: matches) { // Filtering
		auto* match_name = ucsdet_getName(match, &err);
		handle_icu_error(err);
		if (contains(charsets, match_name))
			return match_name;
	}
	return nullopt;
}

auto to_utf8(span<byte const> input, string_view input_charset) -> string
{
	auto contents = string{};
	auto contents_capacity = input.size() * 2; // Most pessimistic case of 100% DBCS
	contents.resize(contents_capacity);
	auto err = U_ZERO_ERROR;
	auto converted = ucnv_convert("UTF-8", string{input_charset}.c_str(),
		contents.data(), contents_capacity,
		reinterpret_cast<char const*>(input.data()), input.size(), &err);
	handle_icu_error(err);
	ASSERT(converted < contents_capacity);
	contents.resize(converted);
	ASSUME(contents[contents.size()] == '\0');
	return contents;
}

auto grapheme_clusters(string_view input) -> generator<string_view>
{
	using UTextResource = unique_resource<UText*, decltype([](UText* ut) noexcept { utext_close(ut); })>;
	auto err = U_ZERO_ERROR;
	auto uinput = UTextResource{utext_openUTF8(nullptr, input.data(), input.size(), &err)};
	handle_icu_error(err);
	auto iter = unique_ptr<::icu::BreakIterator>{
		::icu::BreakIterator::createCharacterInstance(::icu::Locale::getRoot(), err)
	};
	handle_icu_error(err);
	iter->setText(uinput.get(), err);
	handle_icu_error(err);

	auto current = iter->first();
	auto next = current;
	while (next = iter->next(), next != ::icu::BreakIterator::DONE) {
		co_yield input.substr(current, next - current);
		current = next;
	}
}

auto scalars(string_view input) -> generator<char32_t>
{
	auto const* src = reinterpret_cast<uint8_t const*>(input.data());
	auto const len = static_cast<int32_t>(input.size());

	auto i = 0;
	while (i < len) {
		auto c = UChar32{};
		U8_NEXT(src, i, len, c);
		if (c < 0) [[unlikely]]
			co_yield 0xFFFD;
		else
			co_yield c;
	}
}

auto is_whitespace(char32_t scalar) -> bool
{ return u_isUWhiteSpace(scalar); }

}
