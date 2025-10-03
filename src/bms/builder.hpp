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

	// Whole part - measure, fractional part - position within measure.
	using NotePosition = rational<int32>;

	struct HeaderCommand {
		usize line_num;
		string_view header;
		string_view slot;
		string_view value;
	};

	struct ChannelCommand {
		usize line_num;
		NotePosition position;
		string_view channel;
		string_view value;
	};

	struct State {
		using Mapping = unordered_map<string, usize, string_hash>;
		Mapping wav;
		Mapping bpm;

		// Retrieve the slot's index, or register a new one
		static auto get_slot_id(Mapping& map, string_view key) -> usize;
	};

	using HeaderHandlerFunc = void(Builder::*)(HeaderCommand, Chart&, State&);
	unordered_map<string, HeaderHandlerFunc, string_hash> header_handlers;
	using ChannelHandlerFunc = void(Builder::*)(ChannelCommand, Chart&, State);
	unordered_map<string, ChannelHandlerFunc, string_hash> channel_handlers;

	void parse_header(string_view line, usize line_num, Chart&, State&);
	void parse_channel(string_view line, usize line_num, Chart&, State&);

	// Generic handlers
	void handle_header_ignored(HeaderCommand, Chart&, State&) {}
	void handle_header_ignored_log(HeaderCommand, Chart&, State&);
	void handle_header_unimplemented(HeaderCommand, Chart&, State&);
	void handle_header_unimplemented_critical(HeaderCommand, Chart&, State&);
	void handle_channel_ignored(ChannelCommand, Chart&, State&) {}
	void handle_channel_ignored_log(ChannelCommand, Chart&, State&);
	void handle_channel_unimplemented(ChannelCommand, Chart&, State&);
	void handle_channel_unimplemented_critical(ChannelCommand, Chart&, State&);

	// Metadata handlers
	void handle_header_title(HeaderCommand, Chart&, State&);
	void handle_header_subtitle(HeaderCommand, Chart&, State&);
	void handle_header_artist(HeaderCommand, Chart&, State&);
	void handle_header_subartist(HeaderCommand, Chart&, State&);
	void handle_header_genre(HeaderCommand, Chart&, State&);
	void handle_header_url(HeaderCommand, Chart&, State&);
	void handle_header_email(HeaderCommand, Chart&, State&);
	void handle_header_bpm(HeaderCommand, Chart&, State&);
	void handle_header_difficulty(HeaderCommand, Chart&, State&);

	// Slot reference handlers
	void handle_header_wav(HeaderCommand, Chart&, State&);
	void handle_header_bpmxx(HeaderCommand, Chart&, State&);
};

inline Builder::Builder()
{
	// Implemented headers
	header_handlers.emplace("TITLE",        &Builder::handle_header_title);
	header_handlers.emplace("SUBTITLE",     &Builder::handle_header_subtitle);
	header_handlers.emplace("ARTIST",       &Builder::handle_header_artist);
	header_handlers.emplace("SUBARTIST",    &Builder::handle_header_subartist);
	header_handlers.emplace("GENRE",        &Builder::handle_header_genre);
	header_handlers.emplace("%URL",         &Builder::handle_header_url);
	header_handlers.emplace("%EMAIL",       &Builder::handle_header_email);
	header_handlers.emplace("BPM",          &Builder::handle_header_bpm);
	header_handlers.emplace("DIFFICULTY",   &Builder::handle_header_difficulty);
	header_handlers.emplace("WAV",          &Builder::handle_header_wav);

	// Critical unimplemented headers
	// (if a file uses one of these, there is no chance for the BMS to be played correctly)
	header_handlers.emplace("SCROLL",       &Builder::handle_header_unimplemented_critical); // beatoraja extension, needs research, especially for negative values
	header_handlers.emplace("WAVCMD",       &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("EXWAV",        &Builder::handle_header_unimplemented_critical); // Underspecified, and likely unimplementable
	header_handlers.emplace("RANDOM",       &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("IF",           &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("ELSEIF",       &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("ELSE",         &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("ENDIF",        &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("SETRANDOM",    &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("ENDRANDOM",    &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("SWITCH",       &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("CASE",         &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("SKIP",         &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("DEF",          &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("SETSWITCH",    &Builder::handle_header_unimplemented_critical);
	header_handlers.emplace("ENDSW",        &Builder::handle_header_unimplemented_critical);

	// Unimplemented headers
	header_handlers.emplace("VOLWAV",       &Builder::handle_header_unimplemented);
	header_handlers.emplace("STAGEFILE",    &Builder::handle_header_unimplemented);
	header_handlers.emplace("BANNER",       &Builder::handle_header_unimplemented);
	header_handlers.emplace("BACKBMP",      &Builder::handle_header_unimplemented);
	header_handlers.emplace("MAKER",        &Builder::handle_header_unimplemented);
	header_handlers.emplace("COMMENT",      &Builder::handle_header_unimplemented);
	header_handlers.emplace("TEXT",         &Builder::handle_header_unimplemented);
	header_handlers.emplace("SONG",         &Builder::handle_header_unimplemented);
	header_handlers.emplace("EXBPM",        &Builder::handle_header_unimplemented);
	header_handlers.emplace("BASEBPM",      &Builder::handle_header_unimplemented);
	header_handlers.emplace("STOP",         &Builder::handle_header_unimplemented);
	header_handlers.emplace("STP",          &Builder::handle_header_unimplemented);
	header_handlers.emplace("LNTYPE",       &Builder::handle_header_unimplemented);
	header_handlers.emplace("LNOBJ",        &Builder::handle_header_unimplemented);
	header_handlers.emplace("OCT/FP",       &Builder::handle_header_unimplemented);
	header_handlers.emplace("CDDA",         &Builder::handle_header_unimplemented);
	header_handlers.emplace("MIDIFILE",     &Builder::handle_header_unimplemented);
	header_handlers.emplace("BMP",          &Builder::handle_header_unimplemented);
	header_handlers.emplace("BGA",          &Builder::handle_header_unimplemented);
	header_handlers.emplace("@BGA",         &Builder::handle_header_unimplemented);
	header_handlers.emplace("POORBGA",      &Builder::handle_header_unimplemented);
	header_handlers.emplace("SWBGA",        &Builder::handle_header_unimplemented);
	header_handlers.emplace("ARGB",         &Builder::handle_header_unimplemented);
	header_handlers.emplace("VIDEOFILE",    &Builder::handle_header_unimplemented);
	header_handlers.emplace("VIDEOf/s",     &Builder::handle_header_unimplemented);
	header_handlers.emplace("VIDEOCOLORS",  &Builder::handle_header_unimplemented);
	header_handlers.emplace("VIDEODLY",     &Builder::handle_header_unimplemented);
	header_handlers.emplace("MOVIE",        &Builder::handle_header_unimplemented);
	header_handlers.emplace("ExtChr",       &Builder::handle_header_unimplemented);

	// Unsupported headers
	header_handlers.emplace("PLAYER",       &Builder::handle_header_ignored); // Legacy, unreliable
	header_handlers.emplace("RANK",         &Builder::handle_header_ignored); // Playnote enforces uniform judgment
	header_handlers.emplace("DEFEXRANK",    &Builder::handle_header_ignored); // ^
	header_handlers.emplace("EXRANK",       &Builder::handle_header_ignored); // ^
	header_handlers.emplace("TOTAL",        &Builder::handle_header_ignored); // Playnote enforces uniform gauges
	header_handlers.emplace("PLAYLEVEL",    &Builder::handle_header_ignored); // Unreliable and useless
	header_handlers.emplace("DIVIDEPROP",   &Builder::handle_header_ignored); // Not required
	header_handlers.emplace("CHARSET",      &Builder::handle_header_ignored_log); // ^
	header_handlers.emplace("CHARFILE",     &Builder::handle_header_ignored_log); // Unspecified
	header_handlers.emplace("SEEK",         &Builder::handle_header_ignored_log); // ^
	header_handlers.emplace("EXBMP",        &Builder::handle_header_ignored_log); // Underspecified (what's the blending mode?)
	header_handlers.emplace("PATH_WAV",     &Builder::handle_header_ignored_log); // Security concern
	header_handlers.emplace("MATERIALS",    &Builder::handle_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSWAV", &Builder::handle_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSBMP", &Builder::handle_header_ignored_log); // ^
	header_handlers.emplace("OPTION",       &Builder::handle_header_ignored_log); // Horrifying, who invented this
	header_handlers.emplace("CHANGEOPTION", &Builder::handle_header_ignored_log); // ^

	// Implemented channels
	channel_handlers.emplace("01" /* BGM                 */, &Builder::handle_channel_bgm);
	channel_handlers.emplace("02" /* Measure length      */, &Builder::handle_channel_measure_length);
	channel_handlers.emplace("03" /* BPM                 */, &Builder::handle_channel_bpm);
	channel_handlers.emplace("08" /* BPMxx               */, &Builder::handle_channel_bpmxx);
	for (auto const i: views::iota(1zu, 10zu)) // P1 notes
		channel_handlers.emplace(string{"1"} + static_cast<char>('0' + i), &Builder::handle_channel_note);
	for (auto const i: views::iota(0zu, 26zu)) // ^
		channel_handlers.emplace(string{"1"} + static_cast<char>('A' + i), &Builder::handle_channel_note);
	for (auto const i: views::iota(1zu, 10zu)) // P2 notes
		channel_handlers.emplace(string{"2"} + static_cast<char>('0' + i), &Builder::handle_channel_note);
	for (auto const i: views::iota(0zu, 26zu)) // ^
		channel_handlers.emplace(string{"2"} + static_cast<char>('A' + i), &Builder::handle_channel_note);
	for (auto const i: views::iota(1zu, 10zu)) // P1 long notes
		channel_handlers.emplace(string{"5"} + static_cast<char>('0' + i), &Builder::handle_channel_ln);
	for (auto const i: views::iota(0zu, 26zu)) // ^
		channel_handlers.emplace(string{"5"} + static_cast<char>('A' + i), &Builder::handle_channel_ln);
	for (auto const i: views::iota(1zu, 10zu)) // P2 long notes
		channel_handlers.emplace(string{"6"} + static_cast<char>('0' + i), &Builder::handle_channel_ln);
	for (auto const i: views::iota(0zu, 26zu)) // ^
		channel_handlers.emplace(string{"6"} + static_cast<char>('A' + i), &Builder::handle_channel_ln);

	// Unimplemented channels
	channel_handlers.emplace("04" /* BGA base            */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("06" /* BGA poor            */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("07" /* BGA layer           */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("0A" /* BGA layer 2         */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("0B" /* BGA base alpha      */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("0C" /* BGA layer alpha     */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("0D" /* BGA layer 2 alpha   */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("0E" /* BGA poor alpha      */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("99" /* Text                */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("A1" /* BGA base overlay    */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("A2" /* BGA layer overlay   */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("A3" /* BGA layer 2 overlay */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("A4" /* BGA poor overlay    */, &Builder::handle_channel_unimplemented);
	channel_handlers.emplace("A5" /* BGA key-bound       */, &Builder::handle_channel_unimplemented);
	for (auto const i: views::iota(1zu, 10zu))// P1 notes (adlib)
		channel_handlers.emplace(string{"3"} + static_cast<char>('0' + i), &Builder::handle_channel_unimplemented);
	for (auto const i: views::iota(0zu, 26zu)) // ^
		channel_handlers.emplace(string{"3"} + static_cast<char>('A' + i), &Builder::handle_channel_unimplemented);
	for (auto const i: views::iota(1zu, 10zu)) // P2 notes (adlib)
		channel_handlers.emplace(string{"4"} + static_cast<char>('0' + i), &Builder::handle_channel_unimplemented);
	for (auto const i: views::iota(0zu, 26zu)) // ^
		channel_handlers.emplace(string{"4"} + static_cast<char>('A' + i), &Builder::handle_channel_unimplemented);

	// Critical unimplemented channels
	// (if a file uses one of these, there is no chance for the BMS to the played correctly)
	channel_handlers.emplace("09" /* Stop                */, &Builder::handle_channel_unimplemented_critical);
	channel_handlers.emplace("97" /* BGM volume          */, &Builder::handle_channel_unimplemented_critical);
	channel_handlers.emplace("98" /* Key volume          */, &Builder::handle_channel_unimplemented_critical);
	for (auto const i: views::iota(1zu, 10zu)) // P1 mines
		channel_handlers.emplace(string{"D"} + static_cast<char>('0' + i), &Builder::handle_channel_unimplemented_critical);
	for (auto const i: views::iota(1zu, 10zu)) // P2 mines
		channel_handlers.emplace(string{"E"} + static_cast<char>('0' + i), &Builder::handle_channel_unimplemented_critical);

	// Unsupported channels
	channel_handlers.emplace("A0" /* Judge               */, &Builder::handle_channel_ignored);
	channel_handlers.emplace("00" /* Unused              */, &Builder::handle_channel_ignored_log);
	channel_handlers.emplace("05" /* ExtChr, seek        */, &Builder::handle_channel_ignored_log);
	channel_handlers.emplace("A6" /* Play option         */, &Builder::handle_channel_ignored_log);
}

inline auto Builder::build(span<byte const> bms_raw, io::Song& song, optional<reference_wrapper<Metadata>> cache) -> shared_ptr<Chart const>
{
	auto chart = make_shared<Chart>();
	chart->md5 = lib::openssl::md5(bms_raw);
	if (cache) chart->metadata = *cache;
	chart->metadata.density.resolution = 1ms; //TODO remove this workaround once finished
	auto state = State{};

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
		if (line[1] >= '0' && line[1] <= '9')
			parse_channel(line, line_num, *chart, state);
		else
			parse_header(line, line_num, *chart, state);
	}

	return chart;
}

inline auto Builder::State::get_slot_id(Mapping& map, string_view key) -> usize
{
	return map.contains(key)?
		map.at(key) :
		map.emplace(key, map.size()).first->second;
}

inline void Builder::parse_header(string_view line, usize line_num, Chart& chart, State& state)
{
	// Extract components
	auto header = string{substr_until(line, [](auto c) { return c == ' ' || c == '\t'; })};
	to_upper(header);
	if (!header_handlers.contains(header)) {
		WARN("L{}: Unknown header: {}", line_num, header);
		return;
	}
	auto const value = header.size() < line.size()? line.substr(header.size() + 1) : ""sv;

	// Extract slot if applicable
	auto slot = ""s;
	for (auto command: CommandsWithSlots) {
		if (header.starts_with(command)) {
			slot = header.substr(command.size());
			header.resize(command.size());
		}
	}
	// Just in case someone forgot the leading 0
	if (!slot.empty() && slot.size() < 2)
		slot.insert(slot.begin(), 2 - slot.size(), '0');

	// Dispatch
	(this->*header_handlers.at(header))({line_num, header, slot, value}, chart, state);
}

inline void Builder::parse_channel(string_view line, usize line_num, Chart& chart, State& state)
{
	if (line.size() < 4) return; // Not enough space for even the measure
	auto const measure = lexical_cast<int32>(line.substr(1, 3)); // This won't throw (first character is a digit)

	auto const colon_pos = line.find_first_of(':');
	auto channel = string{line.substr(4, colon_pos - 4)};
	to_upper(channel);
	auto value = colon_pos < line.size()? string{line.substr(colon_pos + 1)} : ""s;
	to_upper(value);

	if (channel.empty()) {
		WARN("L{}: Missing measure channel", line_num);
		return;
	}
	// Just in case someone forgot the leading 0
	if (channel.size() < 2)
		channel.insert(channel.begin(), 2 - channel.size(), '0');
	if (!channel_handlers.contains(channel)) {
		WARN("L{}: Unknown channel: {}", line_num, channel);
		return;
	}
	// Truncate value at first whitespace
	value = substr_until(value, [](auto c) { return c == ' ' || c == '\t'; });
	if (value.empty()) {
		WARN("L{}: No valid measure value", line_num);
		return;
	}

	// Dispatch
	auto channel_takes_float = [](string const& ch) { return ch == "02"; };
	if (channel_takes_float(channel)) { // Expected channel value is a single float
		(this->*channel_handlers.at(channel))({line_num, measure, channel, value}, chart, state);
	} else { // Expected channel value is a series of 2-character notes
		// Chop off unpaired characters
		if (value.size() % 2 != 0) {
			WARN("L{}: Stray character in measure: {}", line_num, value.back());
			value.pop_back();
			// This might've emptied the view, but then the loop below will run 0 times
		}

		// Advance 2 chars at a time
		auto numerator = 0zu;
		auto const denominator = value.size() / 2;
		for (auto const note: value | views::chunk(2) | views::to_sv) {
			(this->*channel_handlers.at(channel))({line_num, measure + NotePosition{numerator, denominator}, channel, note}, chart, state);
			numerator += 1;
		}
	}
}

inline void Builder::handle_header_ignored_log(HeaderCommand cmd, Chart&, State&)
{
	INFO("L{}: Ignored header: {}", cmd.line_num, cmd.header);
}

inline void Builder::handle_header_unimplemented(HeaderCommand cmd, Chart&, State&)
{
	WARN("L{}: Unimplemented header: {}", cmd.line_num, cmd.header);
}

inline void Builder::handle_header_unimplemented_critical(HeaderCommand cmd, Chart&, State&)
{
	throw runtime_error_fmt("L{}: Critical unimplemented header: {}", cmd.line_num, cmd.header);
}

inline void Builder::handle_channel_ignored_log(ChannelCommand cmd, Chart&, State&)
{
	INFO("L{}: Ignored channel: {}", cmd.line_num, cmd.channel);
}

inline void Builder::handle_channel_unimplemented(ChannelCommand cmd, Chart&, State&)
{
	WARN("L{}: Unimplemented channel: {}", cmd.line_num, cmd.channel);
}

inline void Builder::handle_channel_unimplemented_critical(ChannelCommand cmd, Chart&, State&)
{
	throw runtime_error_fmt("L{}: Critical unimplemented channel: {}", cmd.line_num, cmd.channel);
}

inline void Builder::handle_header_title(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: Title header has no value", cmd.line_num);
		return;
	}
	chart.metadata.title = cmd.value;
}

inline void Builder::handle_header_subtitle(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: Subtitle header has no value", cmd.line_num);
		return;
	}
	chart.metadata.subtitle = cmd.value;
}

inline void Builder::handle_header_artist(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: Artist header has no value", cmd.line_num);
		return;
	}
	chart.metadata.artist = cmd.value;
}

inline void Builder::handle_header_subartist(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: Subartist header has no value", cmd.line_num);
		return;
	}
	chart.metadata.subartist = cmd.value;
}

inline void Builder::handle_header_genre(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: Genre header has no value", cmd.line_num);
		return;
	}
	chart.metadata.genre = cmd.value;
}

inline void Builder::handle_header_url(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: URL header has no value", cmd.line_num);
		return;
	}
	chart.metadata.url = cmd.value;
}

inline void Builder::handle_header_email(HeaderCommand cmd, Chart& chart, State&)
{
	if (cmd.value.empty()) {
		WARN("L{}: email header has no value", cmd.line_num);
		return;
	}
	chart.metadata.email = cmd.value;
}

inline void Builder::handle_header_bpm(HeaderCommand cmd, Chart& chart, State& state)
try {
	if (!cmd.slot.empty()) {
		handle_header_bpmxx(cmd, chart, state);
		return;
	}
	if (cmd.value.empty()) {
		WARN("L{}: BPM header has no value", cmd.line_num);
		return;
	}
	auto const bpm = lexical_cast<float>(cmd.value);
	chart.metadata.bpm_range.initial = bpm;
}
catch (exception const&) {
	WARN("L{}: BPM header has an invalid value: {}", cmd.line_num, cmd.value);
}

inline void Builder::handle_header_difficulty(HeaderCommand cmd, Chart& chart, State&)
try {
	if (cmd.value.empty()) {
		WARN("L{}: Difficulty header has no value", cmd.line_num);
		return;
	}
	auto const level = lexical_cast<int32>(cmd.value);
	if (level < 1 || level > 5) {
		WARN("L{}: Difficulty header has an invalid value: {}", cmd.line_num, level);
		return;
	}
	chart.metadata.difficulty = static_cast<IR::HeaderEvent::Difficulty::Level>(level);
}
catch (exception const&) {
	WARN("L{}: Difficulty header has an invalid value: {}", cmd.line_num, cmd.value);
}

inline void Builder::handle_header_wav(HeaderCommand cmd, Chart& chart, State& state)
{
	if (cmd.slot.empty()) {
		WARN("L{}: WAV header has no slot", cmd.line_num);
		return;
	}
	if (cmd.value.empty()) {
		WARN("L{}: WAV header has no value", cmd.line_num);
		return;
	}

	// Remove extension and trailing dots
	auto const separator_pos = cmd.value.find_last_of('.');
	if (separator_pos != string::npos) {
		cmd.value = cmd.value.substr(0, separator_pos);
		while (cmd.value.ends_with('.'))
			cmd.value = cmd.value.substr(0, cmd.value.size() - 1);
	}

	auto const slot_id = State::get_slot_id(state.wav, cmd.slot);
	ir.add_header_event(IR::HeaderEvent::WAV{ .slot = slot_id, .name = move(cmd.value) });
}

inline void IRCompiler::parse_header_bpmxx(IR& ir, HeaderCommand&& cmd, SlotMappings& maps)
{
	if (cmd.slot.empty()) {
		WARN_AS(cat, "L{}: BPMxx header has no slot", cmd.line);
		return;
	}
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: BPMxx header has no value", cmd.line);
		return;
	}

	auto const slot_id = maps.get_slot_id(maps.bpm, cmd.slot);
	auto const bpm = lexical_cast<float>(cmd.value);
	TRACE_AS(cat, "L{}: BPMxx: {} -> #{}, {}", cmd.line, cmd.slot, slot_id, cmd.value);
	ir.add_header_event(IR::HeaderEvent::BPMxx{ .slot = slot_id, .bpm = bpm });
}

}
