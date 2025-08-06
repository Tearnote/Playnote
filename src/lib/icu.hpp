/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/icu.cppm:
Wrapper for ICU charset handling.
*/

#pragma once
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>
#include "macros/assert.hpp"
#include "preamble.hpp"
#include "logger.hpp"

namespace playnote::lib::icu {

// Throw runtime_error on any ICU error, other than invalid byte during decoding.
inline void handle_icu_error(UErrorCode err)
{
	if (err == U_ZERO_ERROR) return;
	if (U_SUCCESS(err)) {
		WARN("ICU warning: {}", u_errorName(err));
		return;
	}
	if (err == U_ILLEGAL_CHAR_FOUND) {
		WARN("Illegal character found in file");
		return;
	}
	throw runtime_error_fmt("ICU error: {}", u_errorName(err));
}

// Detect the most likely encoding of a piece of text. If a list of charsets is provided, then
// only charsets belonging to the list will be considered. Returns nullopt on no match.
// Throws runtime_error on ICU failure.
inline auto detect_encoding(span<byte const> input, initializer_list<string_view> charsets = {}) -> optional<string>
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
	auto const matches = std::span{matches_ptr, static_cast<usize>(matches_count)};
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

// Convert text from the provided charset to UTF-8.
// Throws on ICU error; invalid bytes in the input data however will be decoded without error
// as a replacement character.
inline auto to_utf8(span<byte const> input, string_view input_charset) -> string
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

}
