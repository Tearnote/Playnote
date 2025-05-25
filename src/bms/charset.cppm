/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/charset.cppm:
Text encoding detection and conversion.
*/

module;
#include <boost/locale/encoding.hpp>
#include <boost/container_hash/hash.hpp>
#include "compact_enc_det/compact_enc_det.h"
#include "macros/assert.hpp"

export module playnote.bms.charset;

import playnote.preamble;
import playnote.logger;

namespace playnote::bms {

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
	return encoding;
}

// Convert a passage of text from a given encoding to Unicode (UTF-8).
// Any bytes that are invalid in the specified encoding are skipped.
export auto text_to_unicode(span<char const> text, Encoding encoding) -> string
{
	auto encoding_str = [&]() {
		switch (encoding) {
		case Encoding::JAPANESE_SHIFT_JIS: return "Shift_JIS";
		case Encoding::KOREAN_EUC_KR: return "EUC-KR";
		default: return "UTF-8";
		}
	}();
	return boost::locale::conv::to_utf<char>(text.data(), text.data() + text.size(), encoding_str);
}

}
