/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/parser.cppm:
A BMS format parser - turns a complete BMS file into a list of commands.
*/

module;
#include <string_view>
#include <variant>
#include "util/log_macros.hpp"

export module playnote.bms.parser;

import playnote.stx.callable;
import playnote.util.charset;
import playnote.globals;

namespace playnote::bms {

using util::UString;

export struct HeaderCommand {
	UString header;
	UString slot;
	UString value;
};

export struct ChannelCommand {
	UString measure;
	UString channel;
	UString value;
};

auto parse_channel(UString&& command) -> ChannelCommand
{
	return {}; // TODO
}

auto parse_header(UString&& command) -> HeaderCommand
{
	auto first_space = command.indexOf(' ');
	if (first_space == -1) first_space = command.length();
	auto first_tab = command.indexOf('\t');
	if (first_tab == -1) first_tab = command.length();
	auto first_whitespace = std::min(first_space, first_tab);

	auto header = UString{command, 1, first_whitespace - 1};
	header.toUpper();
	auto value = UString{command, first_whitespace + 1};

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
	else if (header.startsWith("SEEK"  )) extract_slot(4);
	else if (header.startsWith("EXBPM" )) extract_slot(5);
	else if (header.startsWith("EXWAV" )) extract_slot(5);
	else if (header.startsWith("SWBGA")) extract_slot(5);
	else if (header.startsWith("EXRANK")) extract_slot(6);
	else if (header.startsWith("CHANGEOPTION")) extract_slot(12);

	return {
		.header = header,
		.slot = std::move(slot),
		.value = std::move(value)
	};
}

auto parse_line(UString&& line) -> std::variant<std::monostate, HeaderCommand, ChannelCommand>
{
	line.trim(); // BMS occasionally uses leading whitespace
	if (line.isEmpty()) return {};
	if (line[0] != '#') return {}; // Ignore comments
	if (line[1] >= '0' && line[1] <= '9')
		return parse_channel(std::move(line));
	else
		return parse_header(std::move(line));
}

export template<
	stx::callable<void(HeaderCommand&&)> HFunc,
	stx::callable<void(ChannelCommand&&)> CFunc
>
void parse(std::string_view path, std::string_view raw_bms_file, HFunc&& header_func, CFunc&& channel_func)
{
	// Convert file to UTF-16
	auto encoding = util::detect_text_encoding(raw_bms_file);
	if (encoding.empty()) {
		L_WARN("Failed to detect encoding, assuming Shift_JIS");
		encoding = "Shift_JIS";
	} else {
		L_TRACE("Detected encoding: {}", encoding);
	}
	auto bms_file_u16 = util::text_to_unicode(raw_bms_file, encoding);

	// Normalize line endings
	bms_file_u16.findAndReplace("\r\n", "\n");
	bms_file_u16.findAndReplace("\r", "\n");

	// Split into lines
	auto pos = int{0};
	auto len = bms_file_u16.length();
	while (pos < len) {
		auto split_pos = bms_file_u16.indexOf('\n', pos);
		if (split_pos == -1) split_pos = len;

		auto result = parse_line({bms_file_u16, pos, split_pos - pos});
		if (std::holds_alternative<HeaderCommand>(result))
			header_func(std::move(std::get<HeaderCommand>(result)));
		else if (std::holds_alternative<ChannelCommand>(result))
			channel_func(std::move(std::get<ChannelCommand>(result)));

		pos = split_pos + 1; // Skip over the newline
	}
}

}
