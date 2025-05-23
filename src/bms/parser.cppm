/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/parser.cppm:
A BMS format parser - turns a complete BMS file into a list of commands.
*/

module;
#include "util/logger.hpp"

export module playnote.bms.parser;

import playnote.preamble;
import playnote.util.charset;
import playnote.util.logger;

namespace playnote::bms {

// A "header" type BMS command
export struct HeaderCommand {
	int line;
	string header;
	string slot;
	string value;
};

// A "channel" type BMS command
export struct ChannelCommand {
	int line;
	int measure;
	string channel;
	string value;
};

// Parse a known "header" type command into its individual components.
// If the command is malformed, measure will be set to -1 which is otherwise an invalid value.
auto parse_channel(int line_index, string&& command) -> ChannelCommand
{
	auto colon_pos = command.find_first_of(':');
	if (colon_pos != 6 || command.length() < 9) return {-1};

	auto measure = lexical_cast<int>(string{command, 1, 3}); // We checked that at least the first character is a digit, so this won't throw
	auto channel = string{command, 4, 2};
	to_upper(channel);
	auto value = string{command, 7};
	to_upper(value);

	return {
		.line = line_index,
		.measure = measure,
		.channel = move(channel),
		.value = move(value),
	};
}

// Parse a known "header" type command into its individual components.
// Some fields might be returned empty if the command is malformed.
auto parse_header(int line_index, string&& command) -> HeaderCommand
{
	auto first_space = command.find_first_of(' ');
	if (first_space == string::npos) first_space = command.length();
	auto first_tab = command.find_first_of('\t');
	if (first_tab == string::npos) first_tab = command.length();
	auto first_whitespace = min(first_space, first_tab);

	auto header = string{command, 1, first_whitespace - 1};
	to_upper(header);
	auto value = first_whitespace < command.size()? string{command, first_whitespace + 1} : "";

	auto slot = string{};
	auto extract_slot = [&](usize start) {
		slot = string{header, start};
		header.resize(start);
	};
	     if (header.starts_with("WAV"   )) extract_slot(3);
	else if (header.starts_with("BMP"   )) extract_slot(3);
	else if (header.starts_with("BGA"   )) extract_slot(3);
	else if (header.starts_with("BPM"   )) extract_slot(3); // This will also match BPM rather than BPMxx, but slot will end up empty anyway
	else if (header.starts_with("TEXT"  )) extract_slot(4);
	else if (header.starts_with("SONG"  )) extract_slot(4);
	else if (header.starts_with("@BGA"  )) extract_slot(4);
	else if (header.starts_with("STOP"  )) extract_slot(4);
	else if (header.starts_with("ARGB"  )) extract_slot(4);
	else if (header.starts_with("SEEK"  )) extract_slot(4);
	else if (header.starts_with("EXBPM" )) extract_slot(5);
	else if (header.starts_with("EXWAV" )) extract_slot(5);
	else if (header.starts_with("SWBGA" )) extract_slot(5);
	else if (header.starts_with("EXRANK")) extract_slot(6);
	else if (header.starts_with("CHANGEOPTION")) extract_slot(12);

	return {
		.line = line_index,
		.header = header,
		.slot = move(slot),
		.value = move(value)
	};
}

// Parse a line into the appropriate command.
// If the line doesn't contain a valid command, std::monostate is returned.
auto parse_line(int line_index, string&& line) -> variant<monostate, HeaderCommand, ChannelCommand>
{
	trim(line); // BMS occasionally uses leading whitespace
	if (line.size() < 2) return {};
	if (line[0] != '#') return {}; // Ignore comments
	if (line[1] >= '0' && line[1] <= '9')
		return parse_channel(line_index, move(line));
	else
		return parse_header(line_index, move(line));
}

// Parse an entire BMS file, running the provided functions once for each command.
// The functions are called in the same order the lines appear in the file.
// The commands might be invalid, and some fields might be empty.
export template<
	callable<void(HeaderCommand&&)> HFunc,
	callable<void(ChannelCommand&&)> CFunc
>
void parse(span<char const> bms_file_contents, util::Logger::Category* cat, HFunc&& header_func, CFunc&& channel_func)
{
	// Convert file to UTF-8
	auto encoding = util::detect_text_encoding(bms_file_contents);
	if (!util::is_supported_encoding(encoding))
		WARN_AS(cat, "Unexpected encoding #{}, proceeding with heuristics", to_underlying(encoding));
	auto bms_file_u8 = util::text_to_unicode(bms_file_contents, encoding);

	// Normalize line endings
	replace_all(bms_file_u8, "\r\n", "\n");
	replace_all(bms_file_u8, "\r", "\n");

	// Split into lines
	auto pos = 0zu;
	auto len = bms_file_u8.length();
	int line_index = 1;
	while (pos < len) {
		auto split_pos = bms_file_u8.find_first_of('\n', pos);
		if (split_pos == string::npos) split_pos = len;

		// Parse line and dispatch
		auto result = parse_line(line_index, {bms_file_u8, pos, split_pos - pos});
		if (holds_alternative<HeaderCommand>(result))
			header_func(move(get<HeaderCommand>(result)));
		else if (holds_alternative<ChannelCommand>(result))
			channel_func(move(get<ChannelCommand>(result)));

		pos = split_pos + 1; // Skip over the newline
		line_index += 1;
	}
}

}
