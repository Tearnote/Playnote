/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

util/charset.cppm:
Text encoding detection and conversion.
*/

module;
#include <string_view>
#include <type_traits>
#include <unicode/errorcode.h>
#include <unicode/unistr.h>
#include <unicode/numfmt.h>
#include <unicode/ucnv.h>
#include <boost/locale/encoding_utf.hpp>
#include <boost/container_hash/hash.hpp>
#include "compact_enc_det/compact_enc_det.h"
#include "libassert/assert.hpp"
#include "util/logger.hpp"

export module playnote.util.charset;

import playnote.preamble;
import playnote.globals;
import playnote.util.logger;

namespace playnote::util {

export using UString = icu::UnicodeString;

// Wrapper enabling UString to be used as a hashmap key
// We don't use UString.hashCode() because it's a low quality 32-bit hash
export class UStringHash {
public:
	using is_avalanching = std::true_type;
	auto operator()(UString const& str) const noexcept -> uint64 {
		using sv_hash = boost::hash<std::basic_string_view<UChar>>;
		return sv_hash{}({str.getBuffer(), static_cast<usize>(str.length())});
	}
};

// Throw on any ICU error other than invalid byte during decoding.
// A BMS file with wrongly detected encoding will have some garbled text, but should still
// be playable.
export auto handle_icu_error(UErrorCode err)
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

// Convert a UString to a std::string.
// Useful for debug output, but do not use carelessly as this allocates.
export auto to_utf8(UString const& str) -> string
{
	return boost::locale::conv::utf_to_utf<char>(str.getBuffer());
}

// Returns true if the encoding is one of the known BMS encodings.
export auto is_supported_encoding(Encoding encoding) -> bool
{
	if (encoding == Encoding::ASCII_7BIT)
		return true;
	if (encoding == Encoding::JAPANESE_SHIFT_JIS)
		return true;
	if (encoding == Encoding::KOREAN_EUC_KR)
		return true;
	return false;
}

// Try to detect the encoding of a passage of text.
// A few encodings known to be common to BMS files are supported. If none of them match,
// an empty string is returned.
export auto detect_text_encoding(span<char const> text) -> Encoding
{
	auto is_reliable{true};
	auto bytes_consumed{0};
	auto encoding = CompactEncDet::DetectEncoding(
		text.data(), text.size(),
		nullptr, nullptr, nullptr,
		UNKNOWN_ENCODING,
		UNKNOWN_LANGUAGE,
		CompactEncDet::QUERY_CORPUS,
		true,
		&bytes_consumed,
		&is_reliable);
	if (is_reliable)
		TRACE("Detected encoding #{}", to_underlying(encoding));
	else
		WARN("Detected encoding #{} (not reliable)", to_underlying(encoding));
	return encoding;
}

// Convert a passage of text from a given encoding to Unicode (UTF-16).
// Any bytes that are invalid in the specified encoding are turned into replacement characters.
export auto text_to_unicode(span<char const> text, Encoding encoding) -> UString
{
	using Converter = unique_ptr<UConverter, decltype([](auto* p) {
		ucnv_close(p);
	})>;

	auto err = U_ZERO_ERROR;
	auto encoding_str = [&]() {
		switch (encoding) {
		case Encoding::JAPANESE_SHIFT_JIS: return "Shift_JIS";
		case Encoding::KOREAN_EUC_KR: return "EUC-KR";
		default: return "UTF-8";
		}
	}();
	auto converter = Converter{ucnv_open(encoding_str, &err)};
	handle_icu_error(err);
	auto contents = UString{};
	auto contents_capacity = text.size() + 1; // Add space for null terminator
	auto contents_buf = contents.getBuffer(contents_capacity);
	auto converted = ucnv_toUChars(converter.get(), contents_buf, contents_capacity, text.data(), text.size(), &err);
	handle_icu_error(err);
	ASSUME(converted < contents_capacity);
	contents.releaseBuffer(converted); // Cuts off the null terminator
	return contents;
}

// Make the global formatters usable anywhere. The resulting instances are thread-safe.
export void init_global_formatters()
{
	auto err = U_ZERO_ERROR;
	g_int_formatter = icu::NumberFormat::createInstance(icu::Locale::getRoot(), err);
	handle_icu_error(err);
	g_int_formatter->setParseIntegerOnly(true);
	g_float_formatter = icu::NumberFormat::createInstance(icu::Locale::getRoot(), err);
	handle_icu_error(err);
}

// Parse a Unicode string as an integer. Parsing begins at the first character, and ends
// with success once the string ends or text is encountered that ends the number. If the string
// begins with a non-number, an std::runtime_error is thrown.
export auto to_int(UString const& str) -> int
{
	ASSUME(g_int_formatter);
	auto fmt = icu::Formattable{};
	auto err = U_ZERO_ERROR;
	g_int_formatter->parse(str, fmt, err);
	handle_icu_error(err);
	ASSUME(fmt.getType() == icu::Formattable::Type::kLong);
	return fmt.getLong();
}

// Parse a Unicode string as a floating-point number. Parsing begins at the first character,
// and ends with success once the string ends or text is encountered that ends the number.
// If the string begins with a non-number, an std::runtime_error is thrown.
// Scientific notation is supported, with an integer exponent.
export auto to_float(UString const& str) -> float
{
	ASSUME(g_float_formatter);
	auto fmt = icu::Formattable{};
	auto err = U_ZERO_ERROR;
	g_float_formatter->parse(str, fmt, err);
	handle_icu_error(err);
	ASSUME(fmt.getType() == icu::Formattable::Type::kLong || fmt.getType() == icu::Formattable::Type::kDouble);
	if (fmt.getType() == icu::Formattable::Type::kLong)
		return static_cast<float>(fmt.getLong());
	else
		return static_cast<float>(fmt.getDouble());
}

}
