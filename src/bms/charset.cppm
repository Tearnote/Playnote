/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/charset.cppm:
Text encoding detection and conversion.
*/

module;
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.bms.charset;

import playnote.preamble;
import playnote.logger;

namespace playnote::bms {

// Throw runtime_error on any ICU error other than invalid byte during decoding.
auto handle_icu_error(UErrorCode err)
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

// Returns true if the encoding is one of the known BMS encodings.
export auto is_supported_encoding(string_view encoding) -> bool
{
	if (encoding == "UTF-8")
		return true;
	if (encoding == "Shift_JIS")
		return true;
	if (encoding == "EUC-KR")
		return true;
	return false;
}

// Try to detect the encoding of a passage of text.
// A few encodings known to be common to BMS files are supported. If none of them match,
// an empty string is returned.
export auto detect_text_encoding(span<byte const> text) -> string_view
{
	using Detector = unique_resource<UCharsetDetector*, decltype([](auto* p) {
		ucsdet_close(p);
	})>;

	auto err = U_ZERO_ERROR;
	auto const detector = Detector{ASSUME_VAL(ucsdet_open(&err))};
	handle_icu_error(err);
	ucsdet_setText(detector.get(), reinterpret_cast<char const*>(text.data()), text.size(), &err);
	handle_icu_error(err);

	auto* matches_ptr = static_cast<UCharsetMatch const**>(nullptr);
	auto matches_count = 0;
	matches_ptr = ucsdet_detectAll(detector.get(), &matches_count, &err);
	handle_icu_error(err);
	auto const matches = std::span{matches_ptr, static_cast<usize>(matches_count)};

	for (auto const& match: matches) {
		auto* match_name = ucsdet_getName(match, &err);
		handle_icu_error(err);
		if (is_supported_encoding(match_name))
			return match_name;
	}
	return "";
}

// Convert a passage of text from a given encoding to Unicode (UTF-8).
// Any bytes that are invalid in the specified encoding are skipped.
export auto text_to_unicode(span<byte const> text, string_view encoding) -> string
{
	using Converter = unique_ptr<UConverter, decltype([](auto* p) {
		ucnv_close(p);
	})>;

	auto contents = string{};
	auto contents_capacity = text.size() * 2; // Most pessimistic case of 100% DBCS
	contents.resize(contents_capacity);
	auto err = U_ZERO_ERROR;
	auto converted = ucnv_convert(string{encoding}.c_str(), "UTF-8",
		contents.data(), contents_capacity,
		reinterpret_cast<char const*>(text.data()), text.size(), &err);
	handle_icu_error(err);
	TRACE("Charset conversion: input {} bytes, encoding {}, reserved {} bytes, needed {} bytes", text.size(), encoding, contents_capacity, converted);
	ASSUME(converted < contents_capacity);
	contents.resize(converted);
	ASSUME(contents[contents.size()] == '\0');
	return contents;
}

}
