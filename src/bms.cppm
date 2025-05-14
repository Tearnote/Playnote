/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms.cppm:
BMS loading and parsing facilities.
*/

module;
#include <string_view>
#include <memory>
#include <span>
#include <unicode/errorcode.h>
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>
#include "libassert/assert.hpp"
#include "util/log_macros.hpp"

export module playnote.bms;

import playnote.stx.except;
import playnote.stx.types;
import playnote.util.file;
import playnote.globals;

namespace playnote {

using stx::usize;

export class BMS {
public:
	explicit BMS(std::string_view path);

	std::string encoding{};

private:
	auto detect_encoding(std::string_view name, std::span<char const> text) -> std::string_view;
	auto is_supported_encoding(std::string_view) -> bool;
};

class ICUError: public icu::ErrorCode {
protected:
	void handleFailure() const override
	{
		if (errorCode == U_ILLEGAL_CHAR_FOUND) {
			L_WARN("Illegal character found in BMS file");
			return;
		}
		throw stx::runtime_error_fmt("ICU error: {}", errorName());
	}
};

BMS::BMS(std::string_view path)
{
	L_DEBUG("Loading BMS file: \"{}\"", path);
	auto bms_file = util::read_file(path);
	encoding = detect_encoding(bms_file.path, bms_file.contents);
	L_TRACE("Detected encoding: {}", encoding);

	using Converter = std::unique_ptr<UConverter, decltype([](auto* p) {
		ucnv_close(p);
	})>;
	auto err = ICUError{};
	auto converter = Converter{ucnv_open(encoding.c_str(), err)};
	auto contents = std::vector<UChar>{};
	contents.resize(bms_file.contents.size() + 1); // ICU wants to null-terminate
	auto converted = ucnv_toUChars(converter.get(), contents.data(), contents.size(), bms_file.contents.data(), bms_file.contents.size(), err);
	ASSUME(converted < contents.size());
	contents.resize(converted);
	L_TRACE("Converted file \"{}\" from {} to UTF-16 ({} -> {})", path, encoding, bms_file.contents.size(), contents.size());
}

auto BMS::detect_encoding(std::string_view name, std::span<char const> text) -> std::string_view
{
	using Detector = std::unique_ptr<UCharsetDetector, decltype([](auto* p) {
		ucsdet_close(p);
	})>;

	auto err = ICUError{};
	auto detector = Detector{ASSUME_VAL(ucsdet_open(err))};
	ucsdet_setText(detector.get(), text.data(), text.size(), err);

	auto* matches_ptr = static_cast<UCharsetMatch const**>(nullptr);
	auto matches_count = int{0};
	matches_ptr = ucsdet_detectAll(detector.get(), &matches_count, err);
	auto matches = std::span{matches_ptr, static_cast<usize>(matches_count)};

	for (auto const& match: matches) {
		auto* match_name = ucsdet_getName(match, err);
		if (is_supported_encoding(match_name))
			return match_name;
	}
	L_WARN("Unknown encoding for \"{}\"; defaulting to Shift_JIS", name);
	return "Shift_JIS";
}

auto BMS::is_supported_encoding(std::string_view encoding) -> bool
{
	if (encoding == "UTF-8")
		return true;
	if (encoding == "Shift_JIS")
		return true;
	if (encoding == "EUC-KR")
		return true;
	return false;
}

}
