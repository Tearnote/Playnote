/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/parser.cppm:
A BMS format parser - turns a complete BMS file into a list of commands.
*/

module;
#include "macros/logger.hpp"
#include "macros/assert.hpp"
#include "preamble.hpp"

export module playnote.bms.parser;

import playnote.logger;
import playnote.lib.icu;

namespace playnote::bms {

namespace icu = lib::icu;

// A "header" type BMS command.
export struct HeaderCommand {
	usize line; // Line counter, for diagnostics
	string header; // Name of the command, uppercased
	string slot; // Slot affected by the command; empty or up to 2 characters
	string value; // Everything that comes after command name and slot, untrimmed
};

// A "channel" type BMS command.
export struct ChannelCommand {
	usize line; // Line counter, for diagnostics
	int32 measure; // Which measure is receiving events, starting at 0
	string channel; // Which channel is receiving events, up to 2 characters
	string value; // Everything after the ':', possibly empty or with odd char count
};

// The list of encodings that are known to be used by BMS files in the wild.
// Encoding detection is limited to this list to help avoid false positives.
static inline constexpr auto KnownEncodings = {"UTF-8"sv, "Shift_JIS"sv, "EUC-KR"sv};

// Convert a BMS of unknown encoding into UTF-8. Output is guaranteed to be valid UTF-8, possibly
// with replacement characters (U+FFFD).
// Throws if ICU throws.
auto raw_to_utf8(Logger::Category* cat, span<byte const> raw_file_contents) -> string
{
	auto encoding = icu::detect_encoding(raw_file_contents, KnownEncodings);
	if (!encoding) {
		WARN_AS(cat, "Found an unexpected encoding; assuming Shift_JIS");
		encoding = "Shift_JIS";
	}
	return icu::to_utf8(raw_file_contents, *encoding);
}

// Ensure all line endings are '\n' (UNIX style).
void normalize_line_endings(string& text)
{
	replace_all(text, "\r\n", "\n");
	replace_all(text, "\r", "\n");
}

// Parse a known "header" type command into its individual components.
// If the command is malformed, measure will be set to -1 which is otherwise an invalid value.
auto parse_channel(string_view command, usize line_index) -> ChannelCommand
{
	if (command.size() < 4) return { .measure = -1 }; // Not enough space for even the measure number
	auto const measure = lexical_cast<int32>(command.substr(1, 3)); // We checked that at least the first character is a digit, so this won't throw

	auto const colon_pos = command.find_first_of(':');
	auto channel = string{command.substr(4, colon_pos - 4)};
	to_upper(channel);
	auto value = colon_pos < command.size()? string{command.substr(colon_pos + 1)} : ""s;
	to_upper(value);

	return {
		.line = line_index,
		.measure = measure,
		.channel = move(channel),
		.value = move(value),
	};
}

// Parse a known "header" type command into its individual components. Some fields might be returned
// empty if the command is malformed. Command is expected to be trimmed, start with '#' and have
// at least 1 more character.
auto parse_header(string_view command, usize line_index) -> HeaderCommand
{
	ASSUME(!command.empty());
	ASSUME(command[0] == '#');
	ASSUME(command.size() >= 2);

	command = command.substr(1); // Trim the '#'
	auto header = string{substr_until(command, [](auto c) { return c == ' ' || c == '\t'; })};
	to_upper(header);
	auto value = header.size() < command.size()? command.substr(header.size() + 1) : ""sv;

	auto slot = string{};
	auto extract_slot = [&](usize start) {
		slot = header.substr(start);
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

	auto value_copy = string{value}; // If constructed in-place, it would happen after header is moved-out-from
	return {
		.line = line_index,
		.header = move(header),
		.slot = move(slot),
		.value = move(value_copy)
	};
}

// Parse a line into the appropriate command.
// If the line doesn't contain a valid command, std::monostate is returned.
auto parse_line(string_view line, usize line_index) -> variant<monostate, HeaderCommand, ChannelCommand>
{
	line = trim_copy(line); // BMS occasionally uses leading whitespace
	if (line.empty()) return {};
	if (line[0] != '#') return {}; // Ignore comments
	if (line.size() < 2) return {}; // Just "#" is meaningless
	if (line[1] >= '0' && line[1] <= '9')
		return parse_channel(line, line_index);
	return parse_header(line, line_index);
}

// Parse an entire BMS file, running the provided functions once for each command. The functions
// are called in the same order the lines appear in the file. Only basic tokenization is done
// and the commands are not validated; some fields might be empty.
export template<
	callable<void(HeaderCommand&&)> HFunc,
	callable<void(ChannelCommand&&)> CFunc
>
void parse(Logger::Category* cat, span<byte const> raw_file_contents, HFunc&& header_func, CFunc&& channel_func)
{
	auto file_contents = raw_to_utf8(cat, raw_file_contents);
	normalize_line_endings(file_contents);
	auto line_index = 1zu;
	for (auto line: file_contents | views::split('\n') | views::to_sv)
		visit(visitor { header_func, channel_func, [](auto&&){} }, parse_line(line, line_index++));
}

}
