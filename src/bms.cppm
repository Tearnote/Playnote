/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms.cppm:
BMS loading and parsing facilities.
*/

module;
#include <functional>
#include "ankerl/unordered_dense.h"
#include "util/log_macros.hpp"

export module playnote.bms;

import playnote.stx.types;
import playnote.util.charset;
import playnote.util.file;
import playnote.globals;

namespace playnote {

using stx::uint;
using util::UStringHash;
using util::UString;
template<typename Key, typename T, typename Hash>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash>;

export class BMS {
public:
	explicit BMS(std::string_view path);

private:
	void parse_file(UString&& file);
	void parse_line(UString&& line);
	void parse_channel(UString&& line);
	void parse_header(UString&& line);

	struct HeaderParams {
		UString slot;
		UString value;
	};
	unordered_map<UString, std::function<void(HeaderParams)>, UStringHash> header_handlers{};

	std::string path;
};

BMS::BMS(std::string_view path):
	path{path}
{
	L_DEBUG("Loading BMS file: \"{}\"", path);
	auto file = util::read_file(path);
	auto encoding = util::detect_text_encoding(file.contents);
	if (encoding.empty()) {
		L_WARN("Failed to detect encoding, assuming Shift_JIS");
		encoding = "Shift_JIS";
	} else {
		L_TRACE("Detected encoding: {}", encoding);
	}
	auto contents = util::text_to_unicode(file.contents, encoding);
	L_TRACE("Converted \"{}\" to UTF-16", path);
	parse_file(std::move(contents));
}

void trace_ustring(UString const& str)
{
	auto str_u8 = std::string{};
	L_TRACE("{}", str.toUTF8String(str_u8));
}

void BMS::parse_file(UString&& file)
{
	// Normalize line endings
	file.findAndReplace("\r\n", "\n");
	file.findAndReplace("\r", "\n");

	auto pos = int{0};
	auto len = file.length();
	while (pos < len) {
		auto split_pos = file.indexOf('\n', pos);
		if (split_pos == -1)
			split_pos = len;
		parse_line(std::move(UString{file, pos, split_pos - pos}));
		pos = split_pos + 1; // Skip over the newline
	}
}

void BMS::parse_line(UString&& line)
{
	line.trim(); // BMS occasionally uses leading whitespace
	if (line.isEmpty()) return;
	if (line[0] != '#') return; // Ignore comments
	if (line[1] >= '0' && line[1] <= '9')
		parse_channel(std::move(line));
	else
		parse_header(std::move(line));
}

void BMS::parse_channel(UString&& line)
{
	// TODO
}

void BMS::parse_header(UString&& line)
{
	auto first_space = line.indexOf(' ');
	if (first_space == -1) first_space = line.length();
	auto first_tab = line.indexOf('\t');
	if (first_tab == -1) first_tab = line.length();
	auto first_whitespace = std::min(first_space, first_tab);

	auto header = UString{line, 1, first_whitespace - 1};
	header.toUpper();
	auto value = UString{line, first_whitespace + 1};

	auto slot = UString{};
	auto extract_slot = [&](int start) {
		slot = UString{header, start};
		header.truncate(start);
	};
	     if (header.startsWith("WAV"   )) extract_slot(3);
	else if (header.startsWith("BMP"   )) extract_slot(3);
	else if (header.startsWith("BGA"   )) extract_slot(3);
	else if (header.startsWith("BPM"   )) extract_slot(3); // This will also match BPM rather than BPMxx, but slot will end up empty anyway
	else if (header.startsWith("TEXT"  )) extract_slot(4);
	else if (header.startsWith("SONG"  )) extract_slot(4);
	else if (header.startsWith("@BGA"  )) extract_slot(4);
	else if (header.startsWith("STOP"  )) extract_slot(4);
	else if (header.startsWith("ARGB"  )) extract_slot(4);
	else if (header.startsWith("EXBPM" )) extract_slot(5);
	else if (header.startsWith("EXRANK")) extract_slot(6);

	try {
		header_handlers.at(header)({std::move(slot), std::move(value)});
	} catch (std::out_of_range const&) {
		auto header_u8 = std::string{};
		L_WARN("Unrecognized header in file \"{}\": {}", path, header.toUTF8String(header_u8));
	}
}

}
