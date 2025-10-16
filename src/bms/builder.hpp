/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/builder.hpp:
Parsing of BMS chart data into a Chart object.
*/

#pragma once
#include "preamble.hpp"
#include "lib/ebur128.hpp"
#include "lib/icu.hpp"
#include "io/song.hpp"
#include "audio/renderer.hpp"
#include "bms/chart.hpp"
#include "threads/task_pool.hpp"

namespace playnote::bms {

class Builder {
public:
	Builder();

	auto build(span<byte const> bms, io::Song&, optional<reference_wrapper<Metadata>> cache = nullopt) -> shared_ptr<Chart const>;

private:
	// Text encodings expected to be found in BMS files.
	static constexpr auto KnownEncodings = {"UTF-8"sv, "Shift_JIS"sv, "EUC-KR"sv};

	// BMS header commands which can be followed up with a slot value as part of the header.
	static constexpr auto CommandsWithSlots = {"WAV"sv, "BMP"sv, "BGA"sv, "BPM"sv, "TEXT"sv, "SONG"sv, "@BGA"sv,
		"STOP"sv, "ARGB"sv, "SEEK"sv, "EXBPM"sv, "EXWAV"sv, "SWBGA"sv, "EXRANK"sv, "CHANGEOPTION"sv};

	// Whole part - measure, fractional part - position within measure.
	using NotePosition = rational<int32>;

	// Parsed BMS header-type command.
	struct HeaderCommand {
		usize line_num;
		string_view header;
		string_view slot;
		string_view value;
	};

	// Parsed BMS channel-type command.
	struct ChannelCommand {
		usize line_num;
		NotePosition position;
		string_view channel;
		string_view value;
	};

	// This is out of RelativeNote so that it's not unique for each template argument.
	struct Simple {};
	struct LNToggle {};
	using RelativeNoteType = variant<Simple, LNToggle>;

	// A note of a chart with its timing information relative to unknowns such as measure length
	// and BPM. LNs are represented as unpaired ends.
	template<typename T>
	struct RelativeNote {
		using Type = RelativeNoteType;

		Type type;
		Lane::Type lane;
		T position;
		usize wav_slot_idx;

		template<variant_alternative<Type> U>
		[[nodiscard]] auto type_is() const -> bool { return holds_alternative<U>(type); }

		template<variant_alternative<Type> U>
		[[nodiscard]] auto params() -> T& { return get<U>(type); }
		template<variant_alternative<Type> U>
		[[nodiscard]] auto params() const -> T const& { return get<U>(type); }
	};

	using MeasureRelNote = RelativeNote<NotePosition>;

	// A BPM change event, measure-relative.
	struct MeasureRelBPM {
		NotePosition position;
		float bpm;
		float scroll_speed;
	};

	// Temporary structures for building the chart.
	struct State {
		// Maps for flattening the slot values into increasing indices.
		template<typename T>
		using Mapping = unordered_map<string, T, string_hash>;

		struct WavSlot {
			usize idx = -1u; // 0-based increasing index
			string filename; // without extension
			bool used = false; // true if any note uses the slot; if false, audio file load can be skipped
		};
		struct BPMSlot {
			usize idx = -1u;
			float bpm;
		};

		Mapping<WavSlot> wav;
		Mapping<BPMSlot> bpm;

		vector<double> measure_lengths;
		vector<MeasureRelBPM> measure_rel_bpms;
		vector<MeasureRelNote> measure_rel_notes;
	};

	using HeaderHandlerFunc = void(Builder::*)(HeaderCommand, Chart&, State&);
	unordered_map<string, HeaderHandlerFunc, string_hash> header_handlers;
	using ChannelHandlerFunc = void(Builder::*)(ChannelCommand, Chart&, State&);
	unordered_map<string, ChannelHandlerFunc, string_hash> channel_handlers;

	[[nodiscard]] static auto slot_hex_to_int(string_view hex) -> usize;
	static void extend_measure_lengths(vector<double>&, usize max_measure);

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

	// Audio channels
	void handle_channel_bgm(ChannelCommand, Chart&, State&);
	void handle_channel_note(ChannelCommand, Chart&, State&);
	void handle_channel_ln(ChannelCommand, Chart&, State&);

	// Timeline control channels
	void handle_channel_measure_length(ChannelCommand, Chart&, State&);
	void handle_channel_bpm(ChannelCommand, Chart&, State&);
	void handle_channel_bpmxx(ChannelCommand, Chart&, State&);
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
	auto parse_state = State{};
	parse_state.measure_lengths.reserve(256); // Arbitrary

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
			parse_channel(line, line_num, *chart, parse_state);
		else
			parse_header(line, line_num, *chart, parse_state);
	}

	// At this point, we have:
	// - chart.metadata fields that correspond directly to header commands
	//     (this purposefully overwrites any previously applied cache)
	// - state slot mappings
	// - state.measure_lengths
	// - state.measure_rel_bpms, missing initial bpm and unsorted
	// - state.measure_rel_notes, unsorted and LNs unpaired

	// The chart generation process uses several ways of anchoring event positions.
	// Measure-relative:
	//   Event positions are an improper fraction, where the whole part is the measure number,
	//   and the fractional part is position within the measure. This is the initial state
	//   of event positions as stored in the BMS file.
	// Beat-relative:
	//   Once measures are eliminated, event positions are a double, where 1.0 is the duration
	//   of a beat. (A measure is 4 beats, unless modified by a measure length command.)
	//   The actual timestamps will still depend on the BPMs throughout the chart.
	// Absolute:
	//   Position is now a timestamp since chart begin. It's also accompanied by a y-position,
	//   which is used for rendering only and takes into account scroll speed changes.
	//   Imagining a chart as a tall ribbon, y-position is a physical distance from the bottom
	//   of the chart, relative to 1.0 being the on-screen height of one initial-BPM beat.

	// Load used audio samples
	chart->media.wav_slots.resize(parse_state.wav.size());
	auto tasks = vector<task<>>{};
	for (auto const& parsed_slot: parse_state.wav | views::values) {
		if (!parsed_slot.used) continue;
		auto& slot = chart->media.wav_slots[parsed_slot.idx];
		tasks.emplace_back(threads::schedule_task([](io::Song& song, vector<lib::Sample>& slot, string filename) -> task<> {
			slot = move(song.load_audio_file(filename));
			co_return;
		}(song, slot, parsed_slot.filename)));
	}
	sync_wait(when_all(move(tasks)));

	// chart.media is now complete

	// Extract useful parse results
	auto& measure_lengths = parse_state.measure_lengths;
	auto& measure_rel_bpms = parse_state.measure_rel_bpms;
	auto& measure_rel_notes = parse_state.measure_rel_notes;

	// Prepare measure_rel_bpms for use
	if (chart->metadata.bpm_range.initial == 0.0f) chart->metadata.bpm_range.initial = 130.0f; // BMS spec default
	// Add initial BPM as the first BPM section. The entire vector needs to be shifted,
	// but we need to preserve ordering, and there shouldn't be too many of these.
	measure_rel_bpms.insert(measure_rel_bpms.begin(), {NotePosition{0}, chart->metadata.bpm_range.initial, 1.0f});
	// stable_sort is our best friend in chart generation, since by the spec overwriting values is allowed
	// and the bottom-most one always wins.
	stable_sort(measure_rel_bpms, [](auto const& a, auto const& b) { return a.position < b.position; });

	// The next stage is to eliminate the abstraction of measures from the data we have so far,
	// transforming them to be based on beats instead.

	// Generate beat-relative measures
	struct BeatRelMeasure {
		double start;
		double length;
	};
	auto const beat_rel_measures = [&] {
		auto result = vector<BeatRelMeasure>{};
		result.reserve(measure_lengths.size());

		auto cursor = 0.0;
		transform(measure_lengths, back_inserter(result), [&](auto length) {
			auto const measure = BeatRelMeasure{
				.start = cursor,
				.length = length * 4.0,
			};
			cursor += measure.length;
			return measure;
		});
		return result;
	}();

	// Convert measure-relative notes to beat-relative
	using BeatRelNote = RelativeNote<double>;
	auto beat_rel_notes = [&] {
		auto result = vector<BeatRelNote>{};
		result.reserve(measure_rel_notes.size());
		transform(measure_rel_notes, back_inserter(result), [&](auto const& note) {
			auto const& measure = beat_rel_measures[trunc(note.position)];
			auto const position = measure.start + measure.length * rational_cast<double>(fract(note.position));
			return BeatRelNote{
				.type = note.type,
				.lane = note.lane,
				.position = position,
				.wav_slot_idx = note.wav_slot_idx,
			};
		});
		return result;
	}();

	// From now on we won't know where measures start, but measure lines still need to be displayed.
	// To handle that, we generate fake notes into a new lane that's silent and unplayable.
	transform(beat_rel_measures, back_inserter(beat_rel_notes), [](auto const& measure) {
		return BeatRelNote{
			.type = Simple{},
			.lane = Lane::Type::MeasureLine,
			.position = measure.start,
		};
	});

	// Convert measure-relative BPM changes to beat-relative
	struct BeatRelBPM {
		double position;
		float bpm;
		float scroll_speed;
	};
	auto const beat_rel_bpms = [&] {
		auto result = vector<BeatRelBPM>{};
		result.reserve(measure_rel_bpms.size());
		transform(measure_rel_bpms, back_inserter(result), [&](auto const& bpm) {
			auto const& measure = beat_rel_measures[trunc(bpm.position)];
			auto const position = measure.start + measure.length * rational_cast<double>(fract(bpm.position));
			return BeatRelBPM{
				.position = position,
				.bpm = bpm.bpm,
				.scroll_speed = bpm.scroll_speed,
			};
		});
		return result;
	}();

	// The chart data is all beat-relative now, and we can begin to convert it into the absolute form.

	// Generate the chart's final, absolute BPM sections
	chart->timeline.bpm_sections = [&] {
		auto result = vector<BPMChange>{};
		result.reserve(beat_rel_bpms.size() + 1);

		// To establish the timestamp of a BPM change, we need to know the BPM of the previous one,
		// and the number of beats since then. So, the first one needs to be handled manually.
		result.emplace_back(BPMChange{
			.position = 0ns,
			.bpm = beat_rel_bpms[0].bpm, // [0] must exist since we added initial BPM as a section earlier
			.y_pos = 0.0,
			.scroll_speed = 1.0f,
		});

		auto cursor = 0ns;
		auto y_cursor = 0.0;
		transform(beat_rel_bpms | views::pairwise, back_inserter(result), [&](auto const& bpm_pair) {
			auto [prev_bpm, bpm] = bpm_pair;
			auto const beats_elapsed = bpm.position - prev_bpm.position;
			auto const time_elapsed = duration_cast<nanoseconds>(beats_elapsed * duration<double>{60.0 / prev_bpm.bpm});
			cursor += time_elapsed;
			y_cursor += beats_elapsed * prev_bpm.scroll_speed;
			auto const bpm_change = BPMChange{
				.position = cursor,
				.bpm = bpm.bpm,
				.y_pos = y_cursor,
				.scroll_speed = bpm.scroll_speed,
			};
			return bpm_change;
		});
		return result;
	}();

	// Convert notes from beat-relative to absolute
	struct AbsPosition {
		nanoseconds timestamp;
		double y_pos;
	};
	using AbsNote = RelativeNote<AbsPosition>;
	auto const abs_notes = [&] {
		auto result = vector<AbsNote>{};
		result.reserve(beat_rel_notes.size());
		transform(beat_rel_notes, back_inserter(result), [&](auto const& note) {
			// Find the BPM section that the note is part of
			auto bpm_section = find_last_if(views::zip(beat_rel_bpms, chart->timeline.bpm_sections), [&](auto const& view) {
				return note.position >= get<0>(view).position;
			});
			ASSERT(!bpm_section.empty());
			auto [beat_rel_bpm, bpm] = *bpm_section.begin();

			auto const beats_since_bpm = note.position - beat_rel_bpm.position;
			auto const time_since_bpm = beats_since_bpm * duration<double>{60.0 / bpm.bpm};
			auto const timestamp = bpm.position + duration_cast<nanoseconds>(time_since_bpm);
			auto const y_pos = bpm.y_pos + beats_since_bpm * bpm.scroll_speed;
			return AbsNote{
				.type = note.type,
				.lane = note.lane,
				.position = AbsPosition{
					.timestamp = timestamp,
					.y_pos = y_pos,
				},
				.wav_slot_idx = note.wav_slot_idx,
			};
		});
		return result;
	}();

	// Split up the unsorted absolute note stream into sorted lanes
	struct LaneState {
		vector<AbsNote> notes;
		vector<AbsNote> ln_ends;
	};
	auto lane_accumulators = array<LaneState, enum_count<Lane::Type>()>{};
	for (auto const& note: abs_notes) {
		auto& accumulator = lane_accumulators[+note.lane];
		if (note.type_is<Simple>())
			accumulator.notes.emplace_back(note);
		else if (note.type_is<LNToggle>())
			accumulator.ln_ends.emplace_back(note);
	}
	for (auto [idx, lane, accumulator]: views::zip(views::iota(0u), chart->timeline.lanes, lane_accumulators)) {
		auto const type = Lane::Type{idx};

		// Add simple notes
		transform(accumulator.notes, back_inserter(lane.notes), [&](auto const& note) {
			return Note{
				.type = Note::Simple{},
				.timestamp = note.position.timestamp,
				.y_pos = note.position.y_pos,
				.wav_slot = note.wav_slot_idx,
			};
		});

		// Pair up LN ends and add
		stable_sort(accumulator.ln_ends, [](auto const& a, auto const& b) { return a.position.timestamp < b.position.timestamp; });
		if (accumulator.ln_ends.size() % 2 != 0) {
			WARN("Unpaired LN ends found; dropping. Chart is most likely invalid or parsed incorrectly");
			accumulator.ln_ends.pop_back();
		}
		transform(accumulator.ln_ends | views::chunk(2), back_inserter(lane.notes), [&](auto ends) {
			auto const ln_length = ends[1].position.timestamp - ends[0].position.timestamp;
			auto const ln_height = ends[1].position.y_pos - ends[0].position.y_pos;
			return Note{
				.type = Note::LN{
					.length = ln_length,
					.height = static_cast<float>(ln_height),
				},
				.timestamp = ends[0].position.timestamp,
				.y_pos = ends[0].position.y_pos,
				.wav_slot = ends[0].wav_slot_idx,
			};
		});

		if (type != Lane::Type::BGM) {
			// Sort with deduplication

			// std::unique keeps the first of the two duplicate elements while we want to keep the second,
			// so we just reverse the range first
			stable_sort(lane.notes, [](auto const& a, auto const& b) {
				return a.timestamp > b.timestamp; // Reverse sort
			});
			auto removed = unique(lane.notes, [](auto const& a, auto const& b) {
				return a.timestamp == b.timestamp;
			});
			auto removed_count = removed.size();
			lane.notes.erase(removed.begin(), removed.end());
			if (removed_count) INFO("Removed {} duplicate notes", removed_count);
			reverse(lane.notes); // Reverse back
		} else {
			// Sort only
			stable_sort(lane.notes, [](auto const& a, auto const& b) {
				return a.timestamp < b.timestamp;
			});
		}
	}

	// Fill in lane meta-information
	for (auto [idx, lane]: chart->timeline.lanes | views::enumerate) {
		auto const type = static_cast<Lane::Type>(idx);
		lane.playable = type != Lane::Type::BGM && type != Lane::Type::MeasureLine;
		lane.visible = type != Lane::Type::BGM;
		lane.audible = type != Lane::Type::MeasureLine;
	}

	// chart.timeline is now complete

	// Some basic entries from chart.metadata were obtained from BMS headers, but certain statistics
	// have to be calculated by analyzing the chart timeline, while some others require rendering out
	// the entire song audio. Even some very fundamental values, like note count, are not part of
	// the BMS file. If a cache of these values was provided, we're good to go; otherwise, they
	// will be calculated now.

	if (cache) return chart; // We applied the cache already at the start

	// Yeah, the *playstyle* needs to be heuristically determined. Seriously. BMS is not a good format.
	chart->metadata.playstyle = [&] {
		auto lanes_used = array<bool, enum_count<Lane::Type>()>{};
		transform(chart->timeline.lanes, lanes_used.begin(), [](auto const& lane) { return !lane.notes.empty(); });
		if (lanes_used[+Lane::Type::P2_Key6] ||
			lanes_used[+Lane::Type::P2_Key7])
			return Playstyle::_14K;
		if (lanes_used[+Lane::Type::P2_Key1] ||
			lanes_used[+Lane::Type::P2_Key2] ||
			lanes_used[+Lane::Type::P2_Key3] ||
			lanes_used[+Lane::Type::P2_Key4] ||
			lanes_used[+Lane::Type::P2_Key5] ||
			lanes_used[+Lane::Type::P2_KeyS])
			return Playstyle::_10K;
		if (lanes_used[+Lane::Type::P1_Key6] ||
			lanes_used[+Lane::Type::P1_Key7])
			return Playstyle::_7K;
		if (lanes_used[+Lane::Type::P1_Key1] ||
			lanes_used[+Lane::Type::P1_Key2] ||
			lanes_used[+Lane::Type::P1_Key3] ||
			lanes_used[+Lane::Type::P1_Key4] ||
			lanes_used[+Lane::Type::P1_Key5] ||
			lanes_used[+Lane::Type::P1_KeyS])
			return Playstyle::_5K;
		return Playstyle::_7K; // Empty chart, but sure whatever
	}();

	chart->metadata.note_count = fold_left(chart->timeline.lanes, 0u, [](auto acc, auto const& lane) {
		return acc + (lane.playable? lane.notes.size() : 0);
	});

	chart->metadata.chart_duration = fold_left(chart->timeline.lanes |
		views::filter([](auto const& lane) { return !lane.notes.empty() && lane.playable; }) |
		views::transform([](auto const& lane) -> Note const& { return lane.notes.back(); }),
		0ns, [](auto acc, Note const& last_note) { // omg not auto??? Actually, avoids dependent template ugliness
			auto note_end = last_note.timestamp;
			if (last_note.type_is<Note::LN>()) note_end += last_note.params<Note::LN>().length;
			return max(acc, note_end);
		}
	);

	// Offline audio render pass, handling all related statistics in one sweep
	auto [loudness, audio_duration] = [&] {
		static constexpr auto BufferSize = 4096zu / sizeof(dev::Sample); // One memory page
		auto renderer = audio::Renderer{chart};
		auto ctx = lib::ebur128::init(globals::mixer->get_audio().get_sampling_rate());
		auto buffer = vector<dev::Sample>{};
		buffer.reserve(BufferSize);

		auto processing = true;
		while (processing) {
			for (auto _: views::iota(0zu, BufferSize)) {
				auto const sample = renderer.advance_one_sample();
				if (sample) {
					buffer.emplace_back(*sample);
				} else {
					processing = false;
					break;
				}
			}
			lib::ebur128::add_frames(ctx, buffer);
			buffer.clear();
		}
		return pair{lib::ebur128::get_loudness(ctx), renderer.get_cursor().get_progress_ns()};
	}();
	chart->metadata.loudness = loudness;
	chart->metadata.audio_duration = audio_duration;

	// Playable note density distribution
	chart->metadata.density = [&] {
		static constexpr auto Resolution = 125ms;
		static constexpr auto Window = 2s;
		static constexpr auto Bandwidth = 3.0f; // In standard deviations
		// Scale back a stretched window, and correct for considering only 3 standard deviations
		static constexpr auto GaussianScale = 1.0f / (Window / 1s) * (1.0f / 0.973f);

		auto result = Metadata::Density{};
		auto const points = chart->metadata.chart_duration / Resolution + 1;
		result.resolution = Resolution;
		result.key.resize(points);
		result.scratch.resize(points);
		result.ln.resize(points);

		// Collect all playable notes
		auto notes_keys = vector<Note>{};
		auto notes_scr = vector<Note>{};
		auto const note_total = fold_left(chart->timeline.lanes, 0u, [](auto sum, auto const& lane) {
			return sum + lane.playable? lane.notes.size() : 0;
		});
		notes_keys.reserve(note_total);
		notes_scr.reserve(note_total);
		for (auto [type, lane]: views::zip(
			views::iota(0u) | views::transform([](auto idx) { return Lane::Type{idx}; }),
			chart->timeline.lanes) |
			views::filter([](auto const& view) { return get<1>(view).playable; })) {
			auto& dest = type == Lane::Type::P1_KeyS || type == Lane::Type::P2_KeyS? notes_scr : notes_keys;
			for (Note const& note: lane.notes) {
				if (note.type_is<Note::LN>()) {
					dest.emplace_back(note);
					auto ln_end = note;
					ln_end.timestamp += ln_end.params<Note::LN>().length;
					dest.emplace_back(ln_end); // LNs count as two notes to make up for increased cognitive load
				} else {
					dest.emplace_back(note);
				}
			}
			}
		sort(notes_keys, [](auto const& left, auto const& right) { return left.timestamp < right.timestamp; });
		sort(notes_scr, [](auto const& left, auto const& right) { return left.timestamp < right.timestamp; });

		auto notes_around = [&](span<Note const> notes, auto cursor) {
			auto const from = cursor - Window;
			auto const to = cursor + Window;
			return notes | views::drop_while([=](auto const& note) {
				return note.timestamp < from;
			}) | views::take_while([=](auto const& note) {
				return note.timestamp <= to;
			});
		};
		for (auto [cursor, key, scratch, ln]: views::zip(
			views::iota(0u) | views::transform([&](auto i) { return i * Resolution; }),
			result.key, result.scratch, result.ln)) {
			for (Note const& note: notes_around(notes_keys, cursor)) {
				auto& target = [&]() -> float& {
					if (note.type_is<Note::LN>()) return ln;
					return key;
				}();
				auto const delta = note.timestamp - cursor;
				auto const delta_scaled = ratio(delta, Window) * Bandwidth; // now within [-Bandwidth, Bandwidth]
				target += exp(-pow(delta_scaled, 2.0f) / 2.0f) * GaussianScale; // Gaussian filter
			}
			for (Note const& note: notes_around(notes_scr, cursor)) {
				auto const delta = note.timestamp - cursor;
				auto const delta_scaled = ratio(delta, Window) * Bandwidth; // now within [-Bandwidth, Bandwidth]
				scratch += exp(-pow(delta_scaled, 2.0f) / 2.0f) * GaussianScale; // Gaussian filter
			}
			}

		return result;
	}();

	// Average notes-per-second
	chart->metadata.nps = [&] {
		auto result = Metadata::NPS{};

		// Average NPS: actually Root Mean Square (4th power) of the middle 50% of the dataset
		auto overall_density = vector<float>{};
		overall_density.reserve(chart->metadata.density.key.size());
		transform(views::zip(chart->metadata.density.key, chart->metadata.density.scratch, chart->metadata.density.ln), back_inserter(overall_density),
			[](auto const& view) { return get<0>(view) + get<1>(view) + get<2>(view); });
		sort(overall_density);
		auto const quarter_size = overall_density.size() / 4;
		auto density_mid50 = span{overall_density.begin() + quarter_size, overall_density.end() - quarter_size};
		auto const rms = fold_left(density_mid50, 0.0,
			[&](auto acc, auto val) { return acc + val * val * val * val / density_mid50.size(); });
		result.average = sqrt(sqrt(rms));

		// Peak NPS: RMS of the 96th percentile
		auto const twentyfifth_size = overall_density.size() / 25;
		auto density_top4 = span{overall_density.end() - twentyfifth_size, overall_density.end()};
		auto const peak_rms = fold_left(density_top4, 0.0,
			[&](auto acc, auto val) { return acc + val * val * val * val / density_top4.size(); });
		result.peak = sqrt(sqrt(peak_rms));

		return result;
	}();

	// Gameplay features in use
	chart->metadata.features = [&] {
		auto result = Metadata::Features{};
		result.has_ln = any_of(chart->timeline.lanes, [](auto const& lane) {
			if (!lane.playable) return false;
			return any_of(lane.notes, [](Note const& note) {
				return note.type_is<Note::LN>();
			});
		});
		result.has_soflan = chart->timeline.bpm_sections.size() > 1;
		return result;
	}();

	// BPM statistics
	chart->metadata.bpm_range = [&] {
		auto bpm_distribution = unordered_map<float, nanoseconds>{};
		auto update = [&](float bpm, nanoseconds duration) {
			bpm_distribution.try_emplace(bpm, 0ns);
			bpm_distribution[bpm] += duration;
		};

		for (auto [current, next]: chart->timeline.bpm_sections | views::pairwise)
			update(current.bpm, next.position - current.position);
		// Last one has no pairing; duration is until the end of the chart
		update(chart->timeline.bpm_sections.back().bpm,
			chart->metadata.chart_duration - chart->timeline.bpm_sections.back().position);

		return Metadata::BPMRange{
			.min = *min_element(bpm_distribution | views::keys),
			.max = *max_element(bpm_distribution | views::keys),
			.main = max_element(bpm_distribution, [](auto const& left, auto const& right) { return left.second < right.second; })->first,
		};
	}();

	return chart;
}

inline auto Builder::slot_hex_to_int(string_view hex) -> usize
{
	auto result = 0zu;
	for (auto const c: hex) {
		if (c >= '0' && c <= '9')
			result = result * 16 + (c - '0');
		else if (c >= 'A' && c <= 'F')
			result = result * 16 + (c - 'A' + 10);
	}
	return result;
}

inline void Builder::extend_measure_lengths(vector<double>& lengths, usize max_measure)
{
	auto const min_length = max_measure + 1;
	if (lengths.size() >= min_length) return;
	lengths.resize(min_length, 1.0);
}

inline void Builder::parse_header(string_view line, usize line_num, Chart& chart, State& state)
{
	// Extract components
	auto header = string{substr_until(line, [](auto c) { return c == ' ' || c == '\t'; })};
	to_upper(header);
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
	if (!header_handlers.contains(header)) {
		WARN("L{}: Unknown header: {}", line_num, header);
		return;
	}
	(this->*header_handlers.at(header))({line_num, header, slot, value}, chart, state);
}

inline void Builder::parse_channel(string_view line, usize line_num, Chart& chart, State& state)
{
	if (line.size() < 3) return; // Not enough space for even the measure
	auto const measure = lexical_cast<int32>(line.substr(0, 3)); // This won't throw (first character is a digit)

	auto const colon_pos = line.find_first_of(':');
	auto channel = string{line.substr(3, colon_pos - 3)};
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
	auto channel_takes_float = [](string_view ch) { return ch == "02"; };
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
	chart.metadata.difficulty = static_cast<Difficulty>(level);
}
catch (exception const&) {
	WARN("L{}: Difficulty header has an invalid value: {}", cmd.line_num, cmd.value);
}

inline void Builder::handle_header_wav(HeaderCommand cmd, Chart&, State& state)
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

	auto& wav_slot = state.wav[cmd.slot];
	if (wav_slot.idx == -1u) wav_slot.idx = state.wav.size() - 1;
	wav_slot.filename = cmd.value;
}

inline void Builder::handle_header_bpmxx(HeaderCommand cmd, Chart&, State& state)
{
	if (cmd.slot.empty()) {
		WARN("L{}: BPMxx header has no slot", cmd.line_num);
		return;
	}
	if (cmd.value.empty()) {
		WARN("L{}: BPMxx header has no value", cmd.line_num);
		return;
	}

	auto& bpm_slot = state.bpm[cmd.slot];
	if (bpm_slot.idx == -1u) bpm_slot.idx = state.bpm.size() - 1;
	bpm_slot.bpm = lexical_cast<float>(cmd.value);
}

inline void Builder::handle_channel_bgm(ChannelCommand cmd, Chart&, State& state)
{
	if (cmd.value == "00") return; // Rest note
	auto const slot_iter = state.wav.find(cmd.value);
	if (slot_iter == state.wav.end()) return; // A BGM note that uses a nonexistent slot does nothing
	auto& slot = slot_iter->second;
	slot.used = true;

	state.measure_rel_notes.emplace_back(MeasureRelNote{
		.type = Simple{},
		.lane = Lane::Type::BGM,
		.position = cmd.position,
		.wav_slot_idx = slot.idx,
	});
	extend_measure_lengths(state.measure_lengths, trunc(cmd.position));
}

inline void Builder::handle_channel_note(ChannelCommand cmd, Chart&, State& state)
{
	if (cmd.value == "00") return; // Rest note
	auto const lane = [&]() {
		if (cmd.channel == "11") return Lane::Type::P1_Key1;
		if (cmd.channel == "12") return Lane::Type::P1_Key2;
		if (cmd.channel == "13") return Lane::Type::P1_Key3;
		if (cmd.channel == "14") return Lane::Type::P1_Key4;
		if (cmd.channel == "15") return Lane::Type::P1_Key5;
		if (cmd.channel == "18") return Lane::Type::P1_Key6;
		if (cmd.channel == "19") return Lane::Type::P1_Key7;
		if (cmd.channel == "16") return Lane::Type::P1_KeyS;
		if (cmd.channel == "21") return Lane::Type::P2_Key1;
		if (cmd.channel == "22") return Lane::Type::P2_Key2;
		if (cmd.channel == "23") return Lane::Type::P2_Key3;
		if (cmd.channel == "24") return Lane::Type::P2_Key4;
		if (cmd.channel == "25") return Lane::Type::P2_Key5;
		if (cmd.channel == "28") return Lane::Type::P2_Key6;
		if (cmd.channel == "29") return Lane::Type::P2_Key7;
		if (cmd.channel == "26") return Lane::Type::P2_KeyS;
		throw runtime_error_fmt("L{}: Unknown note channel: {}", cmd.line_num, cmd.channel);
	}();
	auto const slot_iter = state.wav.find(cmd.value);
	auto slot_idx = -1u;
	if (slot_iter != state.wav.end()) { // Slot doesn't exist; quiet note
		auto& slot = slot_iter->second;
		slot_idx = slot.idx;
		slot.used = true;
	}
	state.measure_rel_notes.emplace_back(MeasureRelNote{
		.type = Simple{},
		.lane = lane,
		.position = cmd.position,
		.wav_slot_idx = slot_idx,
	});
	extend_measure_lengths(state.measure_lengths, trunc(cmd.position));
}

inline void Builder::handle_channel_ln(ChannelCommand cmd, Chart&, State& state)
{
	if (cmd.value == "00") return; // Rest note
	auto const lane = [&]() {
		if (cmd.channel == "51") return Lane::Type::P1_Key1;
		if (cmd.channel == "52") return Lane::Type::P1_Key2;
		if (cmd.channel == "53") return Lane::Type::P1_Key3;
		if (cmd.channel == "54") return Lane::Type::P1_Key4;
		if (cmd.channel == "55") return Lane::Type::P1_Key5;
		if (cmd.channel == "58") return Lane::Type::P1_Key6;
		if (cmd.channel == "59") return Lane::Type::P1_Key7;
		if (cmd.channel == "56") return Lane::Type::P1_KeyS;
		if (cmd.channel == "61") return Lane::Type::P2_Key1;
		if (cmd.channel == "62") return Lane::Type::P2_Key2;
		if (cmd.channel == "63") return Lane::Type::P2_Key3;
		if (cmd.channel == "64") return Lane::Type::P2_Key4;
		if (cmd.channel == "65") return Lane::Type::P2_Key5;
		if (cmd.channel == "68") return Lane::Type::P2_Key6;
		if (cmd.channel == "69") return Lane::Type::P2_Key7;
		if (cmd.channel == "66") return Lane::Type::P2_KeyS;
		throw runtime_error_fmt("L{}: Unknown LN note channel: {}", cmd.line_num, cmd.channel);
	}();
	auto const slot_iter = state.wav.find(cmd.value);
	auto slot_idx = -1u;
	if (slot_iter != state.wav.end()) { // Slot doesn't exist; quiet note
		auto& slot = slot_iter->second;
		slot_idx = slot.idx;
		slot.used = true;
	}
	state.measure_rel_notes.emplace_back(MeasureRelNote{
		.type = LNToggle{},
		.lane = lane,
		.position = cmd.position,
		.wav_slot_idx = slot_idx,
	});
	extend_measure_lengths(state.measure_lengths, trunc(cmd.position));
}

inline void Builder::handle_channel_measure_length(ChannelCommand cmd, Chart&, State& state)
{
	extend_measure_lengths(state.measure_lengths, trunc(cmd.position));
	state.measure_lengths[trunc(cmd.position)] = lexical_cast<double>(cmd.value);
}

inline void Builder::handle_channel_bpm(ChannelCommand cmd, Chart&, State& state)
{
	if (cmd.value == "00") return; // Rhythm padding
	auto const bpm = slot_hex_to_int(cmd.value);
	state.measure_rel_bpms.emplace_back(MeasureRelBPM{
		.position = cmd.position,
		.bpm = static_cast<float>(bpm),
		.scroll_speed = 1.0f,
	});
}

inline void Builder::handle_channel_bpmxx(ChannelCommand cmd, Chart&, State& state)
{
	if (cmd.value == "00") return; // Rhythm padding
	auto const slot_iter = state.bpm.find(cmd.value);
	if (slot_iter == state.bpm.end()) {
		WARN("L{}: Unknown BPM slot", cmd.line_num);
		return;
	}
	auto const bpm = slot_iter->second.bpm;
	if (bpm < 0.0f) {
		WARN("L{}: Invalid BPM value of {}", cmd.line_num, bpm);
		return;
	}
	state.measure_rel_bpms.emplace_back(MeasureRelBPM{
		.position = cmd.position,
		.bpm = bpm,
		.scroll_speed = 1.0f,
	});
}

}
