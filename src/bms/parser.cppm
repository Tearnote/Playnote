/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/parser.cppm:
A BMS format parser - turns a complete BMS file into a list of commands.
*/

module;
#include <string_view>
#include <variant>
#include <span>
#include "util/log_macros.hpp"

export module playnote.bms.parser;

import playnote.preamble;
import playnote.util.charset;
import playnote.globals;

namespace playnote::bms {

using util::UString;
using util::to_int;

// A "header" type BMS command
export struct HeaderCommand {
	int line;
	UString header;
	UString slot;
	UString value;
};

// A "channel" type BMS command
export struct ChannelCommand {
	int line;
	int measure;
	UString channel;
	UString value;
};

// Parse a known "header" type command into its individual components.
// If the command is malformed, measure will be set to -1 which is otherwise an invalid value.
auto parse_channel(int line_index, UString&& command) -> ChannelCommand
{
	auto colon_pos = command.indexOf(':');
	if (colon_pos != 6 || command.length() < 9) return {-1};

	auto measure = to_int({command, 1, 3}); // We checked that at least the first character is a digit, so this won't throw
	auto channel = UString{command, 4, 2};
	channel.toUpper();
	auto value = UString{command, 7};
	value.toUpper();

	return {
		.line = line_index,
		.measure = measure,
		.channel = std::move(channel),
		.value = std::move(value),
	};
}

// Parse a known "header" type command into its individual components.
// Some fields might be returned empty if the command is malformed.
auto parse_header(int line_index, UString&& command) -> HeaderCommand
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
	else if (header.startsWith("SWBGA" )) extract_slot(5);
	else if (header.startsWith("EXRANK")) extract_slot(6);
	else if (header.startsWith("CHANGEOPTION")) extract_slot(12);

	return {
		.line = line_index,
		.header = header,
		.slot = std::move(slot),
		.value = std::move(value)
	};
}

// Parse a line into the appropriate command.
// If the line doesn't contain a valid command, std::monostate is returned.
auto parse_line(int line_index, UString&& line) -> std::variant<std::monostate, HeaderCommand, ChannelCommand>
{
	line.trim(); // BMS occasionally uses leading whitespace
	if (line.isEmpty()) return {};
	if (line[0] != '#') return {}; // Ignore comments
	if (line[1] >= '0' && line[1] <= '9')
		return parse_channel(line_index, std::move(line));
	else
		return parse_header(line_index, std::move(line));
}

// Parse an entire BMS file, running the provided functions once for each command.
// The functions are called in the same order the lines appear in the file.
// The commands might be invalid, and some fields might be empty.
export template<
	callable<void(HeaderCommand&&)> HFunc,
	callable<void(ChannelCommand&&)> CFunc
>
void parse(std::span<char const> bms_file_contents, HFunc&& header_func, CFunc&& channel_func)
{
	// Convert file to UTF-16
	auto encoding = util::detect_text_encoding(bms_file_contents);
	if (encoding.empty()) {
		L_WARN("Failed to detect encoding, assuming Shift_JIS");
		encoding = "Shift_JIS";
	} else {
		L_TRACE("Detected encoding: {}", encoding);
	}
	auto bms_file_u16 = util::text_to_unicode(bms_file_contents, encoding);

	// Normalize line endings
	bms_file_u16.findAndReplace("\r\n", "\n");
	bms_file_u16.findAndReplace("\r", "\n");

	// Split into lines
	auto pos = 0;
	auto len = bms_file_u16.length();
	int line_index = 1;
	while (pos < len) {
		auto split_pos = bms_file_u16.indexOf('\n', pos);
		if (split_pos == -1) split_pos = len;

		// Parse line and dispatch
		auto result = parse_line(line_index, {bms_file_u16, pos, split_pos - pos});
		if (std::holds_alternative<HeaderCommand>(result))
			header_func(std::move(std::get<HeaderCommand>(result)));
		else if (std::holds_alternative<ChannelCommand>(result))
			channel_func(std::move(std::get<ChannelCommand>(result)));

		pos = split_pos + 1; // Skip over the newline
		line_index += 1;
	}
}

}
