/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/task_pool.hpp"
#include "utils/assert.hpp"
#include "utils/logger.hpp"
#include "lib/ebur128.hpp"
#include "lib/openssl.hpp"
#include "lib/icu.hpp"
#include "dev/audio.hpp"
#include "io/song.hpp"
#include "io/file.hpp"
#include "audio/renderer.hpp"
#include "audio/mixer.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

// Generator of charts from BMS files.
class Builder {
public:
	// Create the builder. Chart generation from this builder will use the provided logger.
	explicit Builder(Logger::Category);

	// Build a chart from BMS data. The song must contain audio/video resources referenced by the chart.
	// Optionally, the metadata cache speeds up loading by skipping expensive steps.
	auto build(unique_ptr<thread_pool>&, span<byte const> bms, io::Song&, int sampling_rate,
		optional<reference_wrapper<Metadata>> cache = nullopt) -> task<shared_ptr<Chart const>>;

private:
	// BMS header commands which can be followed up with a slot value as part of the header.
	static constexpr auto CommandsWithSlots = {"WAV"sv, "BMP"sv, "BGA"sv, "BPM"sv, "TEXT"sv, "SONG"sv, "@BGA"sv,
		"STOP"sv, "ARGB"sv, "SEEK"sv, "EXBPM"sv, "EXWAV"sv, "SWBGA"sv, "EXRANK"sv, "CHANGEOPTION"sv};

	// Whole part - measure, fractional part - position within measure.
	using NotePosition = rational<int>;

	// Parsed BMS header-type command.
	struct HeaderCommand {
		isize_t line_num;
		string_view header;
		string_view slot;
		string_view value;
	};

	// Parsed BMS channel-type command.
	struct ChannelCommand {
		isize_t line_num;
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
		isize_t wav_slot_idx;

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
			isize_t idx = -1; // 0-based increasing index
			string filename; // without extension
			bool used = false; // true if any note uses the slot; if false, audio file load can be skipped
		};
		struct BPMSlot {
			isize_t idx = -1;
			float bpm;
		};

		Mapping<WavSlot> wav;
		Mapping<BPMSlot> bpm;

		vector<double> measure_lengths;
		vector<MeasureRelBPM> measure_rel_bpms;
		vector<MeasureRelNote> measure_rel_notes;
	};

	Logger::Category cat;

	using HeaderHandlerFunc = void(Builder::*)(HeaderCommand, Chart&, State&);
	unordered_map<string, HeaderHandlerFunc, string_hash> header_handlers;
	using ChannelHandlerFunc = void(Builder::*)(ChannelCommand, Chart&, State&);
	unordered_map<string, ChannelHandlerFunc, string_hash> channel_handlers;

	[[nodiscard]] static auto slot_hex_to_int(string_view hex) -> isize_t;
	static void extend_measure_lengths(vector<double>&, isize_t max_measure);

	void parse_header(string_view line, isize_t line_num, Chart&, State&);
	void parse_channel(string_view line, isize_t line_num, Chart&, State&);

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

}
