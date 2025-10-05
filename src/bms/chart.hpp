/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart optimized for playback.
*/

#pragma once
#include "preamble.hpp"
#include "dev/audio.hpp"
#include "bms/ir_legacy.hpp"

namespace playnote::bms {

// A note of a chart with a definite timestamp and vertical position, ready for playback.
struct Note {
	struct Simple {};
	struct LN {
		nanoseconds length;
		float height;
	};
	using Type = variant<
		Simple, // 0
		LN      // 1
	>;

	Type type;
	nanoseconds timestamp;
	double y_pos;
	usize wav_slot;

	template<variant_alternative<Type> T>
	[[nodiscard]] auto type_is() const -> bool { return holds_alternative<T>(type); }

	template<variant_alternative<Type> T>
	[[nodiscard]] auto params() -> T& { return get<T>(type); }
	template<variant_alternative<Type> T>
	[[nodiscard]] auto params() const -> T const& { return get<T>(type); }
};

// A column of a chart, with all the notes that will appear on it from start to end.
// Notes are expected to be sorted by timestamp from earliest.
struct Lane {
	// The different types of lanes that a chart can contain.
	enum class Type: usize {
		P1_Key1, P1_Key2, P1_Key3, P1_Key4, P1_Key5, P1_Key6, P1_Key7, P1_KeyS,
		P2_Key1, P2_Key2, P2_Key3, P2_Key4, P2_Key5, P2_Key6, P2_Key7, P2_KeyS,
		BGM, MeasureLine,
	};

	vector<Note> notes;
	bool playable; // Are the notes for the player to hit?
	bool visible; // Are the notes shown on the screen in some way?
	bool audible; // Do the notes trigger audio?
};

// A point in the chart at which the BPM changes.
struct BPMChange {
	nanoseconds position;
	float bpm;
	double y_pos;
	float scroll_speed; // Relative to 1.0 as the BPM's natural scroll speed
};

// A chart's metadata and statistics.
struct Metadata {
	enum class Playstyle {
		_5K, _7K, _9K, _10K, _14K,
	};

	using Difficulty = IR::HeaderEvent::Difficulty::Level;

	// Features used by the chart that the player might want to know about ahead of time.
	struct Features {
		bool has_ln; // Is there at least one LN?
		bool has_soflan; // Are there scroll speed changes?
	};

	// Density functions of a chart's notes.
	struct Density {
		nanoseconds resolution; // real time between values
		vector<float> key;
		vector<float> scratch;
		vector<float> ln;
	};

	// Various notes per seconds statistics.
	struct NPS {
		float average; // The densest the chart gets outside of short bursts
		float peak; // Real peak
	};

	// Statistics about BPMs used in the chart.
	struct BPMRange {
		float initial;
		float min;
		float max;
		float main; // The most common BPM; the mode
	};

	string title;
	string subtitle;
	string artist;
	string subartist;
	string genre;
	string url;
	string email;
	Difficulty difficulty = Difficulty::Unknown;

	Playstyle playstyle;
	Features features;
	uint32 note_count; // Number of notes for the player to hit
	nanoseconds chart_duration; // Timestamp when all notes are judged
	nanoseconds audio_duration; // Timestamp when the last sample stops
	double loudness; // in LUFS

	Density density;
	NPS nps;
	BPMRange bpm_range;
};

using Playstyle = Metadata::Playstyle;
using Difficulty = Metadata::Difficulty;

// All the data that's required to reproduce a chart's timeline (timing and objects.)
struct Timeline {
	using Lanes = array<Lane, enum_count<Lane::Type>()>;

	Lanes lanes;
	vector<BPMChange> bpm_sections; // Sorted from earliest
};

// Media contents referenced by the chart.
struct Media {
	using WavSlot = vector<dev::Sample>;
	vector<WavSlot> wav_slots;
};

// A complete chart. Immutable; a chart is played by creating and advancing a Cursor from it.
struct Chart: enable_shared_from_this<Chart> {
	array<byte, 16> md5;
	Metadata metadata;
	Timeline timeline;
	Media media;
};

}
