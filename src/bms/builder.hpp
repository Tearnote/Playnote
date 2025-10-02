/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/builder.hpp:
Parsing of BMS chart data into a Chart object.
*/

#pragma once
#include "preamble.hpp"
#include "lib/icu.hpp"
#include "io/song.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

class Builder {
public:
	Builder();

	auto build(span<byte const> bms, io::Song&, optional<reference_wrapper<Metadata>> cache = nullopt) -> shared_ptr<Chart const>;

private:
	static constexpr auto KnownEncodings = {"UTF-8"sv, "Shift_JIS"sv, "EUC-KR"sv};
	static constexpr auto CommandsWithSlots = {"WAV"sv, "BMP"sv, "BGA"sv, "BPM"sv, "TEXT"sv, "SONG"sv, "@BGA"sv,
		"STOP"sv, "ARGB"sv, "SEEK"sv, "EXBPM"sv, "EXWAV"sv, "SWBGA"sv, "EXRANK"sv, "CHANGEOPTION"sv};

	void dispatch_header(Chart&, string_view header, string_view slot, string_view value);
	void dispatch_channel(Chart&, string_view channel, string_view value);
};

inline Builder::Builder()
{
	//TODO
}

inline auto Builder::build(span<byte const> bms_raw, io::Song& song, optional<reference_wrapper<Metadata>> cache) -> shared_ptr<Chart const>
{
	auto chart = make_shared<Chart>();
	chart->md5 = lib::openssl::md5(bms_raw);
	if (cache) chart->metadata = *cache;
	chart->metadata.density.resolution = 1ms;

	// Convert chart to UTF-8
	auto encoding = lib::icu::detect_encoding(bms_raw, KnownEncodings);
	if (!encoding) {
		WARN("Unexpected BMS file encoding; assuming Shift_JIS");
		encoding = "Shift_JIS";
	}
	auto bms = lib::icu::to_utf8(bms_raw, *encoding);

	// Normalize line endings
	replace_all(bms, "\r\n", "\n");
	replace_all(bms, "\r", "\n");

	// Parse line-by-line
	for (auto [line_num, line]: views::zip(views::iota(1u), bms | views::split('\n') | views::to_sv)) {
		line = trim_copy(line); // BMS occasionally uses leading whitespace
		if (line.empty()) continue; // Skip empty lines
		if (line[0] != '#') continue; // Anything that doesn't start with "#" is a comment
		line = line.substr(1); // Remove the "#"
		if (line.empty()) continue;
		if (line[1] >= '0' && line[1] <= '9') { // Channel command
			if (line.size() < 4) continue; // Not enough space for even the measure
			auto measure = lexical_cast<int32>(line.substr(1, 3)); // This won't throw (first character is a digit)

			auto const colon_pos = line.find_first_of(':');
			auto const channel = line.substr(4, colon_pos - 4);
			auto const value = colon_pos < line.size()? line.substr(colon_pos + 1) : ""sv;

			dispatch_channel(*chart, channel, value);
		} else { // Header command
			// Extract components
			auto header = substr_until(line, [](auto c) { return c == ' ' || c == '\t'; });
			auto const value = header.size() < line.size()? line.substr(header.size() + 1) : ""sv;

			// Extract slot if applicable
			auto slot_raw = ""sv;
			for (auto command: CommandsWithSlots) {
				if (iequals(header.substr(0, command.size()), command)) {
					slot_raw = header.substr(command.size());
					header = command;
				}
			}
			auto slot = string{slot_raw};
			// Just in case someone forgot the leading 0
			if (!slot.empty() && slot.size() < 2)
				slot.insert(slot.begin(), 2 - slot.size(), '0');

			dispatch_header(*chart, header, slot, value);
		}
	}

	return chart;
}

inline void Builder::dispatch_header(Chart& chart, string_view header, string_view slot, string_view value)
{
	//TODO
}

inline void Builder::dispatch_channel(Chart& chart, string_view channel, string_view value)
{
	//TODO
}

}
