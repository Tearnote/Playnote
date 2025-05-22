/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/ir.cppm:
The Intermediate Representation of a BMS file. Equivalent in functionality to the original file,
but condensed, cleaned and serializable to a binary file.
*/

module;
#include <memory_resource>
#include <string_view>
#include <filesystem>
#include <exception>
#include <utility>
#include <variant>
#include <format>
#include <memory>
#include <ranges>
#include <string>
#include <openssl/evp.h>
#include <boost/rational.hpp>
#include "ankerl/unordered_dense.h"
#include "util/log_macros.hpp"

export module playnote.bms.ir;

import playnote.preamble;
import playnote.stx.callable;
import playnote.stx.concepts;
import playnote.stx.except;
import playnote.util.charset;
import playnote.bms.parser;
import playnote.globals;

namespace playnote::bms {

namespace views = std::ranges::views;
namespace fs = std::filesystem;
template<typename Key, typename T, typename Hash>
using unordered_map = ankerl::unordered_dense::map<Key, T, Hash>;
using util::UStringHash;
using util::UString;
using util::to_float;
using util::to_utf8;
using util::to_int;
using bms::HeaderCommand;

// Whole part - measure, fractional part - position within measure
using NotePosition = boost::rational<int>;

// Print a note position for debug output
export auto to_string(NotePosition pos) -> std::string
{
	return std::format("{} {}/{}",
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
			UString title;
		};
		struct Subtitle {
			UString subtitle;
		};
		struct Artist {
			UString artist;
		};
		struct Subartist {
			UString subartist;
		};
		struct Genre {
			UString genre;
		};
		struct URL {
			UString url;
		};
		struct Email {
			UString email;
		};
		struct Player {
			int count;
		};
		struct BPM {
			float bpm;
		};
		struct WAV {
			int slot;
			UString name;
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
		using ParamsType = std::variant<
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
	template<stx::callable<void(HeaderEvent const&)> Func>
	void each_header_event(Func&& func) const { for (auto const& event: header_events) func(event); }

	// Run the provided function on each channel event, in original file order
	template<stx::callable<void(ChannelEvent const&)> Func>
	void each_channel_event(Func&& func) const { for (auto const& event: channel_events) func(event); }

	// Return the full path of the BMS file that the IR was generated from
	[[nodiscard]] auto get_path() const -> fs::path const& { return path; }

	// Get the total number of WAV slots referenced by the headers and channels
	[[nodiscard]] auto get_wav_slot_count() const -> int { return wav_slot_count; }

private:
	friend IRCompiler;

	// As IR events are typically iterated from start to end, a linear allocator is used when
	// building the structures to maximize cache efficiency
	std::unique_ptr<std::pmr::monotonic_buffer_resource> buffer_resource; // unique_ptr makes it moveable
	std::pmr::polymorphic_allocator<> allocator;

	fs::path path;
	std::array<uint8, 16> md5{};
	std::pmr::vector<HeaderEvent> header_events;
	std::pmr::vector<ChannelEvent> channel_events;

	int wav_slot_count = 0;

	// Only constructible by friends
	IR();

	// Add events, ensuring the IR allocator is used
	template<typename T>
		requires stx::is_variant_alternative<T*, HeaderEvent::ParamsType>
	void add_header_event(T&& event)
	{
		header_events.emplace_back(HeaderEvent{
			.params = allocator.new_object<T>(std::forward<T>(event)),
		});
	}

	void add_channel_event(ChannelEvent&& event)
	{
		channel_events.emplace_back(std::move(event));
	}
};

// Generator of IR from raw BMS file contents.
export class IRCompiler {
public:
	// Initializes internal mappings; reuse instance whenever possible
	IRCompiler();

	// Generate IR from an unmodified BMS file. The path is only used as metadata.
	auto compile(fs::path const& path, std::span<char const> bms_file_contents) -> IR;

private:
	// A BMS channel command can contain multiple notes; we split them up into these
	struct SingleChannelCommand {
		int line;
		NotePosition position;
		UString channel;
		UString value;
	};

	// Tracks the mappings from slot strings to monotonic indices
	struct SlotMappings {
		unordered_map<UString, int, UStringHash> wav;

		// Retrieve the slot's index, or register a new one
		auto get_slot_id(unordered_map<UString, int, UStringHash>& map, UString const& key) -> int;
	};

	// Functions to process each type of header and channel
	using HeaderHandlerFunc = void(IRCompiler::*)(IR&, HeaderCommand&&, SlotMappings&);
	unordered_map<UString, HeaderHandlerFunc, UStringHash> header_handlers{};
	using ChannelHandlerFunc = void(IRCompiler::*)(IR&, SingleChannelCommand&&, SlotMappings&);
	unordered_map<UString, ChannelHandlerFunc, UStringHash> channel_handlers{};

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
	buffer_resource{std::make_unique<std::pmr::monotonic_buffer_resource>()},
	allocator{buffer_resource.get()},
	header_events{allocator},
	channel_events{allocator} {}

IRCompiler::IRCompiler()
{
	register_header_handlers();
	register_channel_handlers();
}

auto IRCompiler::compile(fs::path const& path, std::span<char const> bms_file_contents) -> IR
{
	L_INFO("Compiling BMS file \"{}\"", path.c_str());
	auto ir = IR{};
	auto maps = SlotMappings{};

	// Fill in original metadata to maintain a link from the IR back to the BMS file
	ir.path = path;
	EVP_Q_digest(nullptr, "MD5", nullptr, bms_file_contents.data(), bms_file_contents.size(), ir.md5.data(), nullptr);

	// Process UTF-16 converted and cleanly split BMS file commands
	bms::parse(bms_file_contents,
		[&](HeaderCommand&& cmd) { handle_header(ir, std::move(cmd), maps); },
		[&](ChannelCommand&& cmd) { handle_channel(ir, std::move(cmd), maps); }
	);

	ir.wav_slot_count = maps.wav.size();

	return ir;
}

auto IRCompiler::SlotMappings::get_slot_id(unordered_map<UString, int, UStringHash>& map,
	UString const& key) -> int
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
	channel_handlers.emplace("01" /* BGM                   */, &IRCompiler::parse_channel_bgm);
	for (auto i: views::iota(1, 10)) // P1 notes
		channel_handlers.emplace(UString{"1"}.append('0' + i), &IRCompiler::parse_channel_note);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(UString{"1"}.append('A' + i), &IRCompiler::parse_channel_note);
	for (auto i: views::iota(1, 10)) // P2 notes
		channel_handlers.emplace(UString{"2"}.append('0' + i), &IRCompiler::parse_channel_note);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(UString{"2"}.append('A' + i), &IRCompiler::parse_channel_note);

	// Unimplemented channels
	channel_handlers.emplace("04" /* BGA base              */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("06" /* BGA poor              */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("07" /* BGA layer             */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0A" /* BGA layer 2           */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0B" /* BGA base alpha        */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0C" /* BGA layer alpha       */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0D" /* BGA layer 2 alpha     */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("0E" /* BGA poor alpha        */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("99" /* Text                  */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A1" /* BGA base overlay      */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A2" /* BGA layer overlay     */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A3" /* BGA layer 2 overlay   */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A4" /* BGA poor overlay      */, &IRCompiler::parse_channel_unimplemented);
	channel_handlers.emplace("A5" /* BGA key-bound         */, &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10))// P1 notes (adlib)
		channel_handlers.emplace(UString{"3"}.append('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(UString{"3"}.append('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P2 notes (adlib)
		channel_handlers.emplace(UString{"4"}.append('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(UString{"4"}.append('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P1 long notes
		channel_handlers.emplace(UString{"5"}.append('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(UString{"5"}.append('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P2 long notes
		channel_handlers.emplace(UString{"6"}.append('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(0, 26)) // ^
		channel_handlers.emplace(UString{"6"}.append('A' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P1 mines
		channel_handlers.emplace(UString{"D"}.append('0' + i), &IRCompiler::parse_channel_unimplemented);
	for (auto i: views::iota(1, 10)) // P2 mines
		channel_handlers.emplace(UString{"E"}.append('0' + i), &IRCompiler::parse_channel_unimplemented);

	// Critical unimplemented channels
	// (if a file uses one of these, there is no chance for the BMS to play even remotely correctly)
	channel_handlers.emplace("02" /* Meter                 */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("03" /* BPM                   */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("08" /* BPMxx                 */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("09" /* Stop                  */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("97" /* BGM volume            */, &IRCompiler::parse_channel_unimplemented_critical);
	channel_handlers.emplace("98" /* Key volume            */, &IRCompiler::parse_channel_unimplemented_critical);

	// Unsupported channels
	channel_handlers.emplace("A0" /* Judge                 */, &IRCompiler::parse_channel_ignored);
	channel_handlers.emplace("00" /* Unused                */, &IRCompiler::parse_channel_ignored_log);
	channel_handlers.emplace("05" /* ExtChr, seek          */, &IRCompiler::parse_channel_ignored_log);
	channel_handlers.emplace("A6" /* Play option           */, &IRCompiler::parse_channel_ignored_log);
}

void IRCompiler::handle_header(IR& ir, HeaderCommand&& cmd, SlotMappings& maps)
{
	if (!header_handlers.contains(cmd.header)) {
		L_WARN("L{}: Unknown header: {}", cmd.line, to_utf8(cmd.header));
		return;
	}
	if (!cmd.slot.isEmpty())
		cmd.slot.padLeading(2, '0'); // Just in case someone forgot the leading 0
	(this->*header_handlers.at(cmd.header))(ir, std::move(cmd), maps);
}

void IRCompiler::handle_channel(IR& ir, ChannelCommand&& cmd, SlotMappings& maps)
{
	if (cmd.measure <= 0) {
		L_WARN("L{}: Invalid measure: {}", cmd.line, cmd.measure);
		return;
	}

	if (cmd.channel.isEmpty()) {
		L_WARN("L{}: Missing measure channel", cmd.line);
		return;
	}
	cmd.channel.padLeading(2, '0'); // Just in case someone forgot the leading 0
	if (!channel_handlers.contains(cmd.channel)) {
		L_WARN("L{}: Unknown channel: {}", cmd.line, to_utf8(cmd.channel));
		return;
	}

	// Treat any data not immediately following the ":" as a comment
	cmd.value.truncate(cmd.value.indexOf(' '));
	cmd.value.truncate(cmd.value.indexOf('\t'));
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: No valid measure value", cmd.line);
		return;
	}

	auto channel_takes_float = [](UString const& ch) { return ch == "02"; };
	if (channel_takes_float(cmd.channel)) { // Expected channel value is a single float
		(this->*channel_handlers.at(cmd.channel))(ir, SingleChannelCommand{
			.line = cmd.line,
			.position = cmd.measure,
			.channel = cmd.channel,
			.value = std::move(cmd.value),
		}, maps);
	} else { // Expected channel value is a series of 2-character slots
		// Chop off unpaired characters
		if (cmd.value.length() % 2 != 0) {
			L_WARN("L{}: Stray character in measure: {}", cmd.line, to_utf8(UString{cmd.value, cmd.value.length() - 1}));
			cmd.value.truncate(cmd.value.length() - 1);
		}
		auto denominator = cmd.value.length() / 2;
		for (auto i: views::iota(0, denominator)) {
			(this->*channel_handlers.at(cmd.channel))(ir, SingleChannelCommand{
				.line = cmd.line,
				.position = cmd.measure + NotePosition{i, denominator},
				.channel = cmd.channel,
				.value = UString{cmd.value, i * 2, 2},
			}, maps);
		}
	}
}

void IRCompiler::parse_header_ignored_log(IR&, HeaderCommand&& cmd, SlotMappings&)
{
	L_INFO("L{}: Ignored header: {}", cmd.line, to_utf8(cmd.header));
}

void IRCompiler::parse_header_unimplemented(IR&, HeaderCommand&& cmd, SlotMappings&)
{
	L_WARN("L{}: Unimplemented header: {}", cmd.line, to_utf8(cmd.header));
}

void IRCompiler::parse_header_unimplemented_critical(IR&, HeaderCommand&& cmd, SlotMappings&)
{
	throw stx::runtime_error_fmt("L{}: Critical unimplemented header: {}", cmd.line, to_utf8(cmd.header));
}

void IRCompiler::parse_channel_ignored_log(IR&, SingleChannelCommand&& cmd, SlotMappings&)
{
	L_INFO("L{}: Ignored channel: {}", cmd.line, to_utf8(cmd.channel));
}

void IRCompiler::parse_channel_unimplemented(IR&, SingleChannelCommand&& cmd, SlotMappings&)
{
	L_WARN("L{}: Unimplemented channel: {}", cmd.line, to_utf8(cmd.channel));
}

void IRCompiler::parse_channel_unimplemented_critical(IR&, SingleChannelCommand&& cmd, SlotMappings&)
{
	throw stx::runtime_error_fmt("L{}: Critical unimplemented channel: {}", cmd.line, to_utf8(cmd.channel));
}

void IRCompiler::parse_header_title(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Title header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: Title: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Title{
		.title = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_subtitle(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Subtitle header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: Subtitle: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Subtitle{
		.subtitle = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_artist(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Artist header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: Artist: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Artist{
		.artist = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_subartist(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Subartist header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: Subartist: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Subartist{
		.subartist = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_genre(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Genre header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: Genre: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Genre{
		.genre = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_url(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: URL header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: URL: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::URL{
		.url = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_email(IR& ir, HeaderCommand&& cmd, SlotMappings&)
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: mail header has no value", cmd.line);
		return;
	}

	L_TRACE("L{}: URL: {}", cmd.line, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::Email{
		.email = std::move(cmd.value),
	});
}

void IRCompiler::parse_header_player(IR& ir, HeaderCommand&& cmd, SlotMappings&) try
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Player header has no value", cmd.line);
		return;
	}
	auto count = to_int(cmd.value);
	if (count != 1 && count != 3) { // 1: SP, 3: DP
		L_WARN("L{}: Player header has an invalid value: {}", cmd.line, count);
		return;
	}

	L_TRACE("L{}: Player: {}", cmd.line, count);
	ir.add_header_event(IR::HeaderEvent::Player{
		.count = count,
	});
}
catch (std::exception const&) {
	L_WARN("L{}: Player header has an invalid value: {}", cmd.line, to_utf8(cmd.value));
}

void IRCompiler::parse_header_bpm(IR& ir, HeaderCommand&& cmd, SlotMappings&) try
{
	if (!cmd.slot.isEmpty()) {
		L_WARN("L{}: Unimplemented header: BPMxx", cmd.line);
		return;
	}
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: BPM header has no value", cmd.line);
		return;
	}
	auto bpm = to_float(cmd.value);

	L_TRACE("L{}: BPM: {}", cmd.line, bpm);
	ir.add_header_event(IR::HeaderEvent::BPM{
		.bpm = bpm,
	});
}
catch (std::exception const&) {
	L_WARN("L{}: BPM header has an invalid value: {}", cmd.line, to_utf8(cmd.value));
}

void IRCompiler::parse_header_difficulty(IR& ir, HeaderCommand&& cmd, SlotMappings&) try
{
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: Difficulty header has no value", cmd.line);
		return;
	}
	auto level = to_int(cmd.value);
	if (level < 1 || level > 5) {
		L_WARN("L{}: Difficulty header has an invalid value: {}", cmd.line, level);
		return;
	}

	L_TRACE("L{}: Difficulty: {}", cmd.line, level);
	ir.add_header_event(IR::HeaderEvent::Difficulty{
		.level = static_cast<IR::HeaderEvent::Difficulty::Level>(level),
	});
}
catch (std::exception const&) {
	L_WARN("L{}: Difficulty header has an invalid value: {}", cmd.line, to_utf8(cmd.value));
}

void IRCompiler::parse_header_wav(IR& ir, HeaderCommand&& cmd, SlotMappings& maps)
{
	if (cmd.slot.isEmpty()) {
		L_WARN("L{}: WAV header has no slot", cmd.line);
		return;
	}
	if (cmd.value.isEmpty()) {
		L_WARN("L{}: WAV header has no value", cmd.line);
		return;
	}

	// Remove extension and trailing dots
	auto separator_pos = cmd.value.lastIndexOf('.');
	if (separator_pos != -1) {
		cmd.value.truncate(separator_pos);
		while (cmd.value.endsWith('.'))
			cmd.value.truncate(cmd.value.length() - 1);
	}

	// Treat filenames as canonically lowercase for case-insensitivity
	cmd.value.toLower();

	auto slot_id = maps.get_slot_id(maps.wav, cmd.slot);
	L_TRACE("L{}: WAV: {} -> #{}, {}", cmd.line, to_utf8(cmd.slot), slot_id, to_utf8(cmd.value));
	ir.add_header_event(IR::HeaderEvent::WAV{
		.slot = slot_id,
		.name = std::move(cmd.value),
	});
}

void IRCompiler::parse_channel_bgm(IR& ir, SingleChannelCommand&& cmd, SlotMappings& maps)
{
	if (cmd.value == "00") return;
	auto slot_id = maps.get_slot_id(maps.wav, cmd.value);
	L_TRACE("L{}: {} BGM: {} -> #{}", cmd.line, to_string(cmd.position), to_utf8(cmd.value), slot_id);
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
		throw stx::runtime_error_fmt("L{}: Unknown note channel: {}", cmd.line, to_utf8(cmd.value));
	}();
	auto slot_id = maps.get_slot_id(maps.wav, cmd.value);
	L_TRACE("L{}: {} {}: {} -> #{}", cmd.line, to_string(cmd.position), IR::ChannelEvent::to_string(type), to_utf8(cmd.value), slot_id);
	ir.add_channel_event({
		.position = cmd.position,
		.type = type,
		.slot = slot_id,
	});
}

}
