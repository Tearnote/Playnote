/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/ir.cppm:
The Intermediate Representation of a BMS file. Equivalent in functionality to the original file,
but condensed, cleaned and serializable to a binary file.
*/

module;
#include <openssl/evp.h>
#include <boost/rational.hpp>
#include "util/logger.hpp"

export module playnote.bms.ir;

import playnote.preamble;
import playnote.config;
import playnote.util.charset;
import playnote.util.logger;
import playnote.bms.parser;

namespace playnote::bms {

using bms::HeaderCommand;

// Whole part - measure, fractional part - position within measure
using NotePosition = boost::rational<int>;

// Print a note position for debug output
export auto to_string(NotePosition pos) -> string
{
	return format("{} {}/{}",
		pos.numerator() / pos.denominator(),
		pos.numerator() % pos.denominator(),
		pos.denominator());
}

export class IRCompiler;

// The BMS IR is a list of validated header events and channel events, stored contiguously
// in the same order as the original BMS file. Slots are flattened into sequential indices,
// and each event stores dependencies as an alternative representation of control flow.
export class IR {
public:
	struct HeaderEvent {
		struct Title {
			string title;
		};
		struct Subtitle {
			string subtitle;
		};
		struct Artist {
			string artist;
		};
		struct Subartist {
			string subartist;
		};
		struct Genre {
			string genre;
		};
		struct URL {
			string url;
		};
		struct Email {
			string email;
		};
		struct Player {
			int count;
		};
		struct BPM {
			float bpm;
		};
		struct WAV {
			int slot;
			string name;
		};
		struct Difficulty {
			enum class Level {
				Unknown = 0,
				Beginner,
				Normal,
				Hyper,
				Another,
				Insane,
			};
			Level level;
		};
		using ParamsType = variant<
			std::monostate, //  0
			Title*,         //  1
			Subtitle*,      //  2
			Artist*,        //  3
			Subartist*,     //  4
			Genre*,         //  5
			URL*,           //  6
			Email*,         //  7
			Player*,        //  8
			BPM*,           //  9
			Difficulty*,    // 10
			WAV*            // 11
		>;

		ParamsType params;
	};

	struct ChannelEvent {
		enum class Type {
			Unknown = 0,
			BGM,
			Note_P1_Key1,
			Note_P1_Key2,
			Note_P1_Key3,
			Note_P1_Key4,
			Note_P1_Key5,
			Note_P1_Key6,
			Note_P1_Key7,
			Note_P1_KeyS,
			Note_P2_Key1,
			Note_P2_Key2,
			Note_P2_Key3,
			Note_P2_Key4,
			Note_P2_Key5,
			Note_P2_Key6,
			Note_P2_Key7,
			Note_P2_KeyS,
		};
		static auto to_string(Type) -> char const*;

		NotePosition position;
		Type type;
		int slot;
	};

	// Run the provided function on each header event, in original file order
	template<callable<void(HeaderEvent const&)> Func>
	void each_header_event(Func&& func) const { for (auto const& event: header_events) func(event); }

	// Run the provided function on each channel event, in original file order
	template<callable<void(ChannelEvent const&)> Func>
	void each_channel_event(Func&& func) const { for (auto const& event: channel_events) func(event); }

	// Return the full path of the BMS file that the IR was generated from
	[[nodiscard]] auto get_path() const -> fs::path const& { return path; }

	// Get the total number of WAV slots referenced by the headers and channels
	[[nodiscard]] auto get_wav_slot_count() const -> int { return wav_slot_count; }

private:
	friend IRCompiler;

	// As IR events are typically iterated from start to end, a linear allocator is used when
	// building the structures to maximize cache efficiency
	unique_ptr<pmr::monotonic_buffer_resource> buffer_resource; // unique_ptr makes it moveable
	pmr::polymorphic_allocator<byte> allocator;

	fs::path path;
	array<uint8, 16> md5{};
	pmr::vector<HeaderEvent> header_events;
	pmr::vector<ChannelEvent> channel_events;

	int wav_slot_count = 0;

	// Only constructible by friends
	IR();

	// Add events, ensuring the IR allocator is used
	template<typename T>
		requires is_variant_alternative<T*, HeaderEvent::ParamsType>
	void add_header_event(T&& event)
	{
		auto* event_ptr = static_cast<T*>(buffer_resource->allocate(sizeof(T), alignof(T)));
		allocator.construct(event_ptr, forward<T>(event));
		header_events.emplace_back(HeaderEvent{
			.params = event_ptr,
		});
	}

	void add_channel_event(ChannelEvent&& event)
	{
		channel_events.emplace_back(move(event));
	}
};

// Generator of IR from raw BMS file contents.
export class IRCompiler {
public:
	// Initializes internal mappings; reuse instance whenever possible
	IRCompiler();

	// Generate IR from an unmodified BMS file. The path is only used as metadata.
	auto compile(fs::path const& path, span<char const> bms_file_contents) -> IR;

private:
	// A BMS channel command can contain multiple notes; we split them up into these
	struct SingleChannelCommand {
		int line;
		NotePosition position;
		string channel;
		string value;
	};

	// Tracks the mappings from slot strings to monotonic indices
	struct SlotMappings {
		unordered_map<string, int> wav;

		// Retrieve the slot's index, or register a new one
		auto get_slot_id(unordered_map<string, int>& map, string const& key) -> int;
	};

	util::Logger::Category* cat;

	// Functions to process each type of header and channel
	using HeaderHandlerFunc = void(IRCompiler::*)(IR&, HeaderCommand&&, SlotMappings&);
	unordered_map<string, HeaderHandlerFunc> header_handlers{};
	using ChannelHandlerFunc = void(IRCompiler::*)(IR&, SingleChannelCommand&&, SlotMappings&);
	unordered_map<string, ChannelHandlerFunc> channel_handlers{};

	void register_header_handlers();
	void register_channel_handlers();
	void handle_header(IR&, HeaderCommand&&, SlotMappings&);
	void handle_channel(IR&, ChannelCommand&&, SlotMappings&);

	// Generic handlers
	void parse_header_ignored(IR&, HeaderCommand&&, SlotMappings&) {}
	void parse_header_ignored_log(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_unimplemented(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_unimplemented_critical(IR&, HeaderCommand&&, SlotMappings&);
	void parse_channel_ignored(IR&, SingleChannelCommand&&, SlotMappings&) {}
	void parse_channel_ignored_log(IR&, SingleChannelCommand&&, SlotMappings&);
	void parse_channel_unimplemented(IR&, SingleChannelCommand&&, SlotMappings&);
	void parse_channel_unimplemented_critical(IR&, SingleChannelCommand&&, SlotMappings&);

	// Metadata handlers
	void parse_header_title(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_subtitle(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_artist(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_subartist(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_genre(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_url(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_email(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_player(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_bpm(IR&, HeaderCommand&&, SlotMappings&);
	void parse_header_difficulty(IR&, HeaderCommand&&, SlotMappings&);

	// File reference handlers
	void parse_header_wav(IR&, HeaderCommand&&, SlotMappings&);

	// Audio channels
	void parse_channel_bgm(IR&, SingleChannelCommand&&, SlotMappings&);
	void parse_channel_note(IR&, SingleChannelCommand&&, SlotMappings&);
};

auto IR::ChannelEvent::to_string(Type type) -> char const*
{
	switch (type) {
	case Type::BGM:          return "BGM";
	case Type::Note_P1_Key1: return "Note_P1_Key1";
	case Type::Note_P1_Key2: return "Note_P1_Key2";
	case Type::Note_P1_Key3: return "Note_P1_Key3";
	case Type::Note_P1_Key4: return "Note_P1_Key4";
	case Type::Note_P1_Key5: return "Note_P1_Key5";
	case Type::Note_P1_Key6: return "Note_P1_Key6";
	case Type::Note_P1_Key7: return "Note_P1_Key7";
	case Type::Note_P1_KeyS: return "Note_P1_KeyS";
	case Type::Note_P2_Key1: return "Note_P2_Key1";
	case Type::Note_P2_Key2: return "Note_P2_Key2";
	case Type::Note_P2_Key3: return "Note_P2_Key3";
	case Type::Note_P2_Key4: return "Note_P2_Key4";
	case Type::Note_P2_Key5: return "Note_P2_Key5";
	case Type::Note_P2_Key6: return "Note_P2_Key6";
	case Type::Note_P2_Key7: return "Note_P2_Key7";
	case Type::Note_P2_KeyS: return "Note_P2_KeyS";
	default: return "";
	}
}

IR::IR():
	buffer_resource{make_unique<pmr::monotonic_buffer_resource>()},
	allocator{buffer_resource.get()},
	header_events{allocator},
	channel_events{allocator} {}

IRCompiler::IRCompiler()
{
	cat = globals::logger->register_category("BMS parse", LogLevelBMSBuild);
	register_header_handlers();
	register_channel_handlers();
}

auto IRCompiler::compile(fs::path const& path, span<char const> bms_file_contents) -> IR
{
	INFO_AS(cat, "Compiling BMS file \"{}\"", path.c_str());
	auto ir = IR{};
	auto maps = SlotMappings{};

	// Fill in original metadata to maintain a link from the IR back to the BMS file
	ir.path = path;
	EVP_Q_digest(nullptr, "MD5", nullptr, bms_file_contents.data(), bms_file_contents.size(), ir.md5.data(), nullptr);

	// Process UTF-16 converted and cleanly split BMS file commands
	bms::parse(bms_file_contents, cat,
		[&](HeaderCommand&& cmd) { handle_header(ir, move(cmd), maps); },
		[&](ChannelCommand&& cmd) { handle_channel(ir, move(cmd), maps); }
	);

	ir.wav_slot_count = maps.wav.size();

	return ir;
}

auto IRCompiler::SlotMappings::get_slot_id(unordered_map<string, int>& map,
	string const& key) -> int
{
	return map.contains(key)?
		map.at(key) :
		map.emplace(key, map.size()).first->second;
}

void IRCompiler::register_header_handlers()
{
	// Implemented headers
	header_handlers.emplace("TITLE",        &IRCompiler::parse_header_title);
	header_handlers.emplace("SUBTITLE",     &IRCompiler::parse_header_subtitle);
	header_handlers.emplace("ARTIST",       &IRCompiler::parse_header_artist);
	header_handlers.emplace("SUBARTIST",    &IRCompiler::parse_header_subartist);
	header_handlers.emplace("GENRE",        &IRCompiler::parse_header_genre);
	header_handlers.emplace("%URL",         &IRCompiler::parse_header_url);
	header_handlers.emplace("%EMAIL",       &IRCompiler::parse_header_email);
	header_handlers.emplace("PLAYER",       &IRCompiler::parse_header_player);
	header_handlers.emplace("BPM",          &IRCompiler::parse_header_bpm);
	header_handlers.emplace("DIFFICULTY",   &IRCompiler::parse_header_difficulty);
	header_handlers.emplace("WAV",          &IRCompiler::parse_header_wav);

	// Critical unimplemented headers
	// (if a file uses one of these, there is no chance for the BMS to play even remotely correctly)
	header_handlers.emplace("WAVCMD",       &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("EXWAV",        &IRCompiler::parse_header_unimplemented_critical); // Underspecified, and likely unimplementable
	header_handlers.emplace("RANDOM",       &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("IF",           &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ELSEIF",       &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ELSE",         &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDIF",        &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SETRANDOM",    &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDRANDOM",    &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SWITCH",       &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("CASE",         &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SKIP",         &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("DEF",          &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("SETSWITCH",    &IRCompiler::parse_header_unimplemented_critical);
	header_handlers.emplace("ENDSW",        &IRCompiler::parse_header_unimplemented_critical);

	// Unimplemented headers
	header_handlers.emplace("VOLWAV",       &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("STAGEFILE",    &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BANNER",       &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BACKBMP",      &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("MAKER",        &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("COMMENT",      &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("TEXT",         &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("SONG",         &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("EXBPM",        &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BASEBPM",      &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("STOP",         &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("STP",          &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("LNTYPE",       &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("LNOBJ",        &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("OCT/FP",       &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("CDDA",         &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("MIDIFILE",     &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BMP",          &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("BGA",          &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("@BGA",         &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("POORBGA",      &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("SWBGA",        &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("ARGB",         &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEOFILE",    &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEOf/s",     &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEOCOLORS",  &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("VIDEODLY",     &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("MOVIE",        &IRCompiler::parse_header_unimplemented);
	header_handlers.emplace("ExtChr",       &IRCompiler::parse_header_unimplemented);

	// Unsupported headers
	header_handlers.emplace("RANK",         &IRCompiler::parse_header_ignored); // Playnote enforces uniform judgment
	header_handlers.emplace("DEFEXRANK",    &IRCompiler::parse_header_ignored); // ^
	header_handlers.emplace("EXRANK",       &IRCompiler::parse_header_ignored); // ^
	header_handlers.emplace("TOTAL",        &IRCompiler::parse_header_ignored); // Playnote enforces uniform gauges
	header_handlers.emplace("PLAYLEVEL",    &IRCompiler::parse_header_ignored); // Unreliable and useless
	header_handlers.emplace("DIVIDEPROP",   &IRCompiler::parse_header_ignored); // Not required
	header_handlers.emplace("CHARSET",      &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("CHARFILE",     &IRCompiler::parse_header_ignored_log); // Unspecified
	header_handlers.emplace("SEEK",         &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("EXBMP",        &IRCompiler::parse_header_ignored_log); // Underspecified (what's the blending mode?)
	header_handlers.emplace("PATH_WAV",     &IRCompiler::parse_header_ignored_log); // Security concern
	header_handlers.emplace("MATERIALS",    &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSWAV", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("MATERIALSBMP", &IRCompiler::parse_header_ignored_log); // ^
	header_handlers.emplace("OPTION",       &IRCompiler::parse_header_ignored_log); // Horrifying, who invented this
	header_handlers.emplace("CHANGEOPTION", &IRCompiler::parse_header_ignored_log); // ^
}

void IRCompiler::register_channel_handlers()
{
	// Implemented channels
	channel_handlers.emplace("01" /* BGM                 */, &IRCompiler::parse_channel_bgm);
	for (auto i: views::iota(1, 10)) // P1 notes
		channel_handlers.emplace(string{"1"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_note);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(string{"1"} + static_cast<char>('A' + i), &IRCompiler::parse_channel_note);
	for (auto i: views::iota(1, 10)) // P2 notes
		channel_handlers.emplace(string{"2"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_note);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(string{"2"} + static_cast<char>('A' + i), &IRCompiler::parse_channel_note);

	// Unimplemented channels
	channel_handlers.emplace("04" /* BGA base            */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("06" /* BGA poor            */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("07" /* BGA layer           */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0A" /* BGA layer 2         */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0B" /* BGA base alpha      */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0C" /* BGA layer alpha     */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0D" /* BGA layer 2 alpha   */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0E" /* BGA poor alpha      */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("99" /* Text                */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A1" /* BGA base overlay    */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A2" /* BGA layer overlay   */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A3" /* BGA layer 2 overlay */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A4" /* BGA poor overlay    */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A5" /* BGA key-bound       */, &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10))// P1 notes (adlib)
		channel_handlers.emplace(string{"3"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(string{"3"} + static_cast<char>('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P2 notes (adlib)
		channel_handlers.emplace(string{"4"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(string{"4"} + static_cast<char>('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P1 long notes
		channel_handlers.emplace(string{"5"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(string{"5"} + static_cast<char>('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P2 long notes
		channel_handlers.emplace(string{"6"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(string{"6"} + static_cast<char>('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P1 mines
		channel_handlers.emplace(string{"D"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P2 mines
		channel_handlers.emplace(string{"E"} + static_cast<char>('0' + i), &IRCompiler::parse_channel_unimplemented);

	// Critical unimplemented channels
	// (if a file uses one of these, there is no chance for the BMS to play even remotely correctly)
	channel_handlers.emplace("02" /* Meter               */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("03" /* BPM                 */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("08" /* BPMxx               */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("09" /* Stop                */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("97" /* BGM volume          */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("98" /* Key volume          */, &IRCompiler::parse_channel_unimplemented_critical);

	// Unsupported channels
	channel_handlers.emplace("A0" /* Judge               */, &IRCompiler::parse_channel_ignored);
	channel_handlers.emplace("00" /* Unused              */, &IRCompiler::parse_channel_ignored_log);
	channel_handlers.emplace("05" /* ExtChr, seek        */, &IRCompiler::parse_channel_ignored_log);
	channel_handlers.emplace("A6" /* Play option         */, &IRCompiler::parse_channel_ignored_log);
}

void IRCompiler::handle_header(IR& ir, HeaderCommand&& cmd, SlotMappings& maps)
{
	if (!header_handlers.contains(cmd.header)) {
		WARN_AS(cat, "L{}: Unknown header: {}", cmd.line, cmd.header);
		return;
	}
	if (cmd.slot.size() < 2)
		cmd.slot.insert(0, 2 - cmd.slot.size(), '0'); // Just in case someone forgot the leading 0
	(this->*header_handlers.at(cmd.header))(ir, move(cmd), maps);
}

void IRCompiler::handle_channel(IR& ir, ChannelCommand&& cmd, SlotMappings& maps)
{
	if (cmd.measure < 0) {
		WARN_AS(cat, "L{}: Invalid measure: {}", cmd.line, cmd.measure);
		return;
	}

	if (cmd.channel.empty()) {
		WARN_AS(cat, "L{}: Missing measure channel", cmd.line);
		return;
	}
	if (cmd.channel.size() < 2)
		cmd.channel.insert(0, 2 - cmd.channel.size(), '0'); // Just in case someone forgot the leading 0
	if (!channel_handlers.contains(cmd.channel)) {
		WARN_AS(cat, "L{}: Unknown channel: {}", cmd.line, cmd.channel);
		return;
	}

	// Treat any data not immediately following the ":" as a comment
	auto first_space = cmd.value.find_first_of(' ');
	if (first_space != string::npos)
		cmd.value.resize(first_space);
	auto first_tab = cmd.value.find_first_of('\t');
	if (first_tab != string::npos)
		cmd.value.resize(first_tab);
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: No valid measure value", cmd.line);
		return;
	}

	auto channel_takes_float = [](string const& ch) { return ch == "02"; };
	if (channel_takes_float(cmd.channel)) { // Expected channel value is a single float
		(this->*channel_handlers.at(cmd.channel))(ir, SingleChannelCommand{
			.line = cmd.line,
			.position = cmd.measure,
			.channel = cmd.channel,
			.value = move(cmd.value),
		}, maps);
	} else { // Expected channel value is a series of 2-character slots
		// Chop off unpaired characters
		if (cmd.value.length() % 2 != 0) {
			WARN_AS(cat, "L{}: Stray character in measure: {}", cmd.line, string{cmd.value, cmd.value.length() - 1});
			cmd.value.resize(cmd.value.length() - 1);
		}
		auto denominator = cmd.value.length() / 2;
		for (auto i: views::iota(0zu, denominator)) {
			(this->*channel_handlers.at(cmd.channel))(ir, SingleChannelCommand{
				.line = cmd.line,
				.position = cmd.measure + NotePosition{i, denominator},
				.channel = cmd.channel,
				.value = string{cmd.value, i * 2, 2},
			}, maps);
		}
	}
}

void IRCompiler::parse_header_ignored_log(IR&, HeaderCommand&& cmd, SlotMappings&)
{
	INFO_AS(cat, "L{}: Ignored header: {}", cmd.line, cmd.header);
}

void IRCompiler::parse_header_unimplemented(IR&, HeaderCommand&& cmd, SlotMappings&)
{
	WARN_AS(cat, "L{}: Unimplemented header: {}", cmd.line, cmd.header);
}

void IRCompiler::parse_header_unimplemented_critical(IR&, HeaderCommand&& cmd, SlotMappings&)
{
	throw runtime_error_fmt("L{}: Critical unimplemented header: {}", cmd.line, cmd.header);
}

void IRCompiler::parse_channel_ignored_log(IR&, SingleChannelCommand&& cmd, SlotMappings&)
{
	INFO_AS(cat, "L{}: Ignored channel: {}", cmd.line, cmd.channel);
}

void IRCompiler::parse_channel_unimplemented(IR&, SingleChannelCommand&& cmd, SlotMappings&)
{
	WARN_AS(cat, "L{}: Unimplemented channel: {}", cmd.line, cmd.channel);
}

void IRCompiler::parse_channel_unimplemented_critical(IR&, SingleChannelCommand&& cmd, SlotMappings&)
{
//	throw runtime_error_fmt("L{}: Critical unimplemented channel: {}", cmd.line, cmd.channel);
}

void IRCompiler::parse_header_title(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Title header has no value", cmd.line);
		return;
	}

	TRACE_AS(cat, "L{}: Title: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::Title{
		.title = move(cmd.value),
	});
}

void IRCompiler::parse_header_subtitle(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Subtitle header has no value", cmd.line);
		return;
	}

	TRACE_AS(cat, "L{}: Subtitle: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::Subtitle{
		.subtitle = move(cmd.value),
	});
}

void IRCompiler::parse_header_artist(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Artist header has no value", cmd.line);
		return;
	}

	TRACE_AS(cat, "L{}: Artist: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::Artist{
		.artist = move(cmd.value),
	});
}

void IRCompiler::parse_header_subartist(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Subartist header has no value", cmd.line);
		return;
	}

	TRACE_AS(cat, "L{}: Subartist: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::Subartist{
		.subartist = move(cmd.value),
	});
}

void IRCompiler::parse_header_genre(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Genre header has no value", cmd.line);
		return;
	}

	TRACE_AS(cat, "L{}: Genre: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::Genre{
		.genre = move(cmd.value),
	});
}

void IRCompiler::parse_header_url(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: URL header has no value", cmd.line);
		return;
	}

	TRACE("L{}: URL: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::URL{
		.url = move(cmd.value),
	});
}

void IRCompiler::parse_header_email(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: mail header has no value", cmd.line);
		return;
	}

	TRACE_AS(cat, "L{}: URL: {}", cmd.line, cmd.value);
	ir.add_header_event(IR::HeaderEvent::Email{
		.email = move(cmd.value),
	});
}

void IRCompiler::parse_header_player(IR& ir, HeaderCommand&& cmd, SlotMappings&) try
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Player header has no value", cmd.line);
		return;
	}
	auto count = lexical_cast<int>(cmd.value);
	if (count != 1 && count != 3) { // 1: SP, 3: DP
		WARN_AS(cat, "L{}: Player header has an invalid value: {}", cmd.line, count);
		return;
	}

	TRACE_AS(cat, "L{}: Player: {}", cmd.line, count);
	ir.add_header_event(IR::HeaderEvent::Player{
		.count = count,
	});
}
catch (exception const&) {
	WARN_AS(cat, "L{}: Player header has an invalid value: {}", cmd.line, cmd.value);
}

void IRCompiler::parse_header_bpm(IR& ir, HeaderCommand&& cmd, SlotMappings&) try
{
	if (!cmd.slot.empty()) {
		WARN_AS(cat, "L{}: Unimplemented header: BPMxx", cmd.line);
		return;
	}
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: BPM header has no value", cmd.line);
		return;
	}
	auto bpm = lexical_cast<float>(cmd.value);

	TRACE_AS(cat, "L{}: BPM: {}", cmd.line, bpm);
	ir.add_header_event(IR::HeaderEvent::BPM{
		.bpm = bpm,
	});
}
catch (exception const&) {
	WARN_AS(cat, "L{}: BPM header has an invalid value: {}", cmd.line, cmd.value);
}

void IRCompiler::parse_header_difficulty(IR& ir, HeaderCommand&& cmd, SlotMappings&) try
{
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: Difficulty header has no value", cmd.line);
		return;
	}
	auto level = lexical_cast<int>(cmd.value);
	if (level < 1 || level > 5) {
		WARN_AS(cat, "L{}: Difficulty header has an invalid value: {}", cmd.line, level);
		return;
	}

	TRACE_AS(cat, "L{}: Difficulty: {}", cmd.line, level);
	ir.add_header_event(IR::HeaderEvent::Difficulty{
		.level = static_cast<IR::HeaderEvent::Difficulty::Level>(level),
	});
}
catch (std::exception const&) {
	WARN_AS(cat, "L{}: Difficulty header has an invalid value: {}", cmd.line, cmd.value);
}

void IRCompiler::parse_header_wav(IR& ir, HeaderCommand&& cmd, SlotMappings& maps)
{
	if (cmd.slot.empty()) {
		WARN_AS(cat, "L{}: WAV header has no slot", cmd.line);
		return;
	}
	if (cmd.value.empty()) {
		WARN_AS(cat, "L{}: WAV header has no value", cmd.line);
		return;
	}

	// Remove extension and trailing dots
	auto separator_pos = cmd.value.find_last_of('.');
	if (separator_pos != string::npos) {
		cmd.value.resize(separator_pos);
		while (cmd.value.ends_with('.'))
			cmd.value.resize(cmd.value.length() - 1);
	}

	// Treat filenames as canonically lowercase for case-insensitivity
	to_lower(cmd.value);

	auto slot_id = maps.get_slot_id(maps.wav, cmd.slot);
	TRACE_AS(cat, "L{}: WAV: {} -> #{}, {}", cmd.line, cmd.slot, slot_id, cmd.value);
	ir.add_header_event(IR::HeaderEvent::WAV{
		.slot = slot_id,
		.name = move(cmd.value),
	});
}

void IRCompiler::parse_channel_bgm(IR& ir, SingleChannelCommand&& cmd, SlotMappings& maps)
{
	if (cmd.value == "00") return;
	auto slot_id = maps.get_slot_id(maps.wav, cmd.value);
	TRACE_AS(cat, "L{}: {} BGM: {} -> #{}", cmd.line, to_string(cmd.position), cmd.value, slot_id);
	ir.add_channel_event({
		.position = cmd.position,
		.type = IR::ChannelEvent::Type::BGM,
		.slot = slot_id,
	});
}

void IRCompiler::parse_channel_note(IR& ir, SingleChannelCommand&& cmd, SlotMappings& maps)
{
	if (cmd.value == "00") return;
	auto type = [&]() {
		if (cmd.channel == "11") return IR::ChannelEvent::Type::Note_P1_Key1;
		if (cmd.channel == "12") return IR::ChannelEvent::Type::Note_P1_Key2;
		if (cmd.channel == "13") return IR::ChannelEvent::Type::Note_P1_Key3;
		if (cmd.channel == "14") return IR::ChannelEvent::Type::Note_P1_Key4;
		if (cmd.channel == "15") return IR::ChannelEvent::Type::Note_P1_Key5;
		if (cmd.channel == "18") return IR::ChannelEvent::Type::Note_P1_Key6;
		if (cmd.channel == "19") return IR::ChannelEvent::Type::Note_P1_Key7;
		if (cmd.channel == "16") return IR::ChannelEvent::Type::Note_P1_KeyS;
		if (cmd.channel == "21") return IR::ChannelEvent::Type::Note_P2_Key1;
		if (cmd.channel == "22") return IR::ChannelEvent::Type::Note_P2_Key2;
		if (cmd.channel == "23") return IR::ChannelEvent::Type::Note_P2_Key3;
		if (cmd.channel == "24") return IR::ChannelEvent::Type::Note_P2_Key4;
		if (cmd.channel == "25") return IR::ChannelEvent::Type::Note_P2_Key5;
		if (cmd.channel == "28") return IR::ChannelEvent::Type::Note_P2_Key6;
		if (cmd.channel == "29") return IR::ChannelEvent::Type::Note_P2_Key7;
		if (cmd.channel == "26") return IR::ChannelEvent::Type::Note_P2_KeyS;
		throw runtime_error_fmt("L{}: Unknown note channel: {}", cmd.line, cmd.value);
	}();
	auto slot_id = maps.get_slot_id(maps.wav, cmd.value);
	TRACE_AS(cat, "L{}: {} {}: {} -> #{}", cmd.line, to_string(cmd.position), IR::ChannelEvent::to_string(type), cmd.value, slot_id);
	ir.add_channel_event({
		.position = cmd.position,
		.type = type,
		.slot = slot_id,
	});
}

}
