/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/charset.cppm:
Text encoding detection and conversion.
*/

module;
#include <string_view>
#include <string>
#include <vector>
#include <span>
#include <unicode/errorcode.h>
#include <unicode/ucsdet.h>
#include <unicode/unistr.h>
#include <unicode/numfmt.h>
#include <unicode/ucnv.h>
#include "libassert/assert.hpp"
#include "ankerl/unordered_dense.h"
#include "util/log_macros.hpp"

export module playnote.util.charset;

import playnote.stx.except;
import playnote.stx.types;
import playnote.globals;

namespace playnote::util {

export using Encoding = std::string;
export using UString = icu::UnicodeString;
using stx::uint64;
using stx::usize;

// Wrapper enabling UString to be used as a hashmap key
// We don't use UString.hashCode() because it's a low quality 32-bit hash
export class UStringHash {
public:
	using is_avalanching = void;
	auto operator()(UString const& str) const noexcept -> uint64 {
		using sv_hash = ankerl::unordered_dense::hash<std::basic_string_view<UChar>>;
		return sv_hash{}({str.getBuffer(), static_cast<usize>(str.length())});
	}
};

// Throw on any ICU error other than invalid byte during decoding.
// A BMS file with wrongly detected encoding will have some garbled text, but should still
// be playable.
export class ICUError: public icu::ErrorCode {
protected:
	void handleFailure() const override
	{
		if (errorCode == U_ILLEGAL_CHAR_FOUND) {
			L_WARN("Illegal character found in file");
			return;
		}
		throw stx::runtime_error_fmt("ICU error: {}", errorName());
	}
};

// Convert a UString to a std::string.
// Useful for debug output, but do not use carelessly as this allocates.
export auto to_utf8(UString const& str) -> std::string
{
	auto result = std::string{};
	return str.toUTF8String(result);
}

// Returns true if the encoding is one of the known BMS encodings
auto is_supported_encoding(std::string_view encoding) -> bool
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
export auto detect_text_encoding(std::span<char const> text) -> Encoding
{
	using Detector = std::unique_ptr<UCharsetDetector, decltype([](auto* p) {
		ucsdet_close(p);
	})>;

	auto detector = Detector{ASSUME_VAL(ucsdet_open(ICUError{}))};
	ucsdet_setText(detector.get(), text.data(), text.size(), ICUError{});

	auto* matches_ptr = static_cast<UCharsetMatch const**>(nullptr);
	auto matches_count = int{0};
	matches_ptr = ucsdet_detectAll(detector.get(), &matches_count, ICUError{});
	auto matches = std::span{matches_ptr, static_cast<usize>(matches_count)};

	for (auto const& match: matches) {
		auto* match_name = ucsdet_getName(match, ICUError{});
		if (is_supported_encoding(match_name))
			return match_name;
	}
	return "";
}

// Convert a passage of text from a given encoding to Unicode (UTF-16).
// Any bytes that are invalid in the specified encoding are turned into replacement characters.
export auto text_to_unicode(std::span<char const> text, Encoding const& encoding) -> UString
{
	using Converter = std::unique_ptr<UConverter, decltype([](auto* p) {
		ucnv_close(p);
	})>;

	auto converter = Converter{ucnv_open(encoding.c_str(), ICUError{})};
	auto contents = UString{};
	auto contents_capacity = text.size() + 1; // Add space for null terminator
	auto contents_buf = contents.getBuffer(contents_capacity);
	auto converted = ucnv_toUChars(converter.get(), contents_buf, contents_capacity, text.data(), text.size(), ICUError{});
	ASSUME(converted < contents_capacity);
	contents.releaseBuffer(converted); // Cuts off the null terminator
	return contents;
}

// Make the global formatters usable anywhere. The resulting instances are thread-safe.
export void init_global_formatters()
{
	g_int_formatter = icu::NumberFormat::createInstance(icu::Locale::getRoot(), ICUError{});
	g_int_formatter->setParseIntegerOnly(true);
}

}
