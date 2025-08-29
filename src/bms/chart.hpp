/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart optimized for playback.
*/

#pragma once
#include "preamble.hpp"
#include "dev/audio.hpp"
#include "bms/ir.hpp"

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

// A list of all possible metadata about a chart.
struct Metadata {
	using Difficulty = IR::HeaderEvent::Difficulty::Level;
	string title;
	string subtitle;
	string artist;
	string subartist;
	string genre;
	string url;
	string email;
	Difficulty difficulty = Difficulty::Unknown;

	[[nodiscard]] static auto to_str(Difficulty diff) -> string_view
	{
		switch (diff) {
		case Difficulty::Beginner: return "Beginner";
		case Difficulty::Normal:   return "Normal";
		case Difficulty::Hyper:    return "Hyper";
		case Difficulty::Another:  return "Another";
		case Difficulty::Insane:   return "Insane";
		default:                   return "Unknown";
		}
	}
};

enum class Playstyle {
	_5K,
	_7K,
	_9K,
	_10K,
	_14K,
};

// Density functions of a chart's notes, and calculated NPS values.
struct Density {
	nanoseconds resolution; // real time between values
	vector<float> key_density;
	vector<float> scratch_density;
	vector<float> ln_density;
	float average_nps; // The densest the chart gets outside of short bursts
	float peak_nps; // Real peak
};

// Features used by the chart that the player might want to know about ahead of time.
struct Features {
	bool has_ln; // Is there at least one LN?
	bool has_soflan; // Are there scroll speed changes?
};

// Summary of a chart's BPM range.
struct BPMRange {
	float min;
	float max;
	float main; // The most common BPM; the mode
	float scroll_adjustment; // Scroll speed multiplier to normalize the chart to 120 BPM
};

// Data about a chart calculated from its contents.
struct Metrics {
	Playstyle playstyle;
	uint32 note_count; // Only counts notes for the player to hit
	nanoseconds chart_duration; // Time when all notes are judged
	nanoseconds audio_duration; // Time until the last sample stops
	double loudness; // in LUFS
	float gain; // Amplitude ratio to normalize loudness to -14 LUFS reference
	Density density;
	Features features;
	BPMRange bpm;
};

// Bounding box acceleration structure for reachable WAV slots.
struct SlotBB {
	static constexpr auto WindowSize = 250ms; // Length of each window
	static constexpr auto MaxSlots = 256zu; // Maximum number of WAV slots in each window
	vector<static_vector<usize, MaxSlots>> windows;
};

// An entire loaded chart, with all of its notes and meta information. Immutable; a chart is played
// by creating and advancing a Play from it.
struct Chart: enable_shared_from_this<Chart> {
	enum class LaneType: usize {
		P1_Key1,
		P1_Key2,
		P1_Key3,
		P1_Key4,
		P1_Key5,
		P1_Key6,
		P1_Key7,
		P1_KeyS,
		P2_Key1,
		P2_Key2,
		P2_Key3,
		P2_Key4,
		P2_Key5,
		P2_Key6,
		P2_Key7,
		P2_KeyS,
		BGM,
		MeasureLine,
		Size,
	};
	using Lanes = array<Lane, +LaneType::Size>;
	using WavSlot = vector<dev::Sample>;

	Metadata metadata;
	Metrics metrics;
	Lanes lanes;
	vector<BPMChange> bpm_changes; // Sorted from earliest
	vector<WavSlot> wav_slots;
	SlotBB slot_bb;
	float bpm = 130.0f; // BMS spec default
};

}
