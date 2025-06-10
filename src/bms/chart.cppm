/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart optimized for playback.
*/

module;
#include "macros/logger.hpp"
#include "macros/assert.hpp"

export module playnote.bms.chart;

import playnote.preamble;
import playnote.logger;
import playnote.lib.pipewire;
import playnote.io.audio_codec;
import playnote.bms.ir;

namespace playnote::bms {

// A note of a chart with a definite timestamp and vertical position, ready for playback.
export struct Note {
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
export struct Lane {
	vector<Note> notes;
	bool playable; // Are the notes for the player to hit?
};

// A list of all possible metadata about a chart.
export struct Metadata {
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

// Data about a chart calculated from its contents.
export struct Metrics {
	uint32 note_count;
	double loudness; // in LUFS
	float gain; // Amplitude ratio to normalize loudness to -14 LUFS reference
};

// An entire loaded chart, with all of its notes and meta information. Immutable; a chart is played
// by creating and advancing a Play from it.
export struct Chart: enable_shared_from_this<Chart> {
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
		Size,
	};

	Metadata metadata;
	Metrics metrics;
	array<Lane, +LaneType::Size> lanes;
	vector<io::AudioCodec::Output> wav_slots;
	float bpm = 130.0f; // BMS spec default
};

// Representation of a moment in chart's progress.
export class Cursor {
public:
	explicit Cursor(Chart const& chart) noexcept;

	[[nodiscard]] auto get_chart() const noexcept -> Chart const& { return *chart; }

	void restart() noexcept;

	template<callable<void(lib::pw::Sample)> Func>
	auto advance_one_sample(Func&& func) noexcept -> bool;

	template<callable<void(Note const&, Chart::LaneType, float)> Func>
	void upcoming_notes(float max_units, Func&& func) const noexcept;

private:
	struct LaneProgress {
		usize next_note; // Index of the earliest note that hasn't been judged yet
		bool ln_active; // Is it currently in the middle of an LN?
		void restart() { next_note = 0; ln_active = false; }
	};
	struct WavSlotProgress {
		static constexpr auto Stopped = -1zu; // Special value for stopped playback
		usize playback_pos = Stopped; // Samples played so far
	};
	shared_ptr<Chart const> chart;
	usize sample_progress = 0zu;
	usize notes_judged = 0zu;
	array<LaneProgress, +Chart::LaneType::Size> lane_progress = {};
	vector<WavSlotProgress> wav_slot_progress;

	[[nodiscard]] static auto samples_to_ns(usize) noexcept -> nanoseconds;
};

template<callable<void(lib::pw::Sample)> Func>
auto Cursor::advance_one_sample(Func&& func) noexcept -> bool
{
	auto chart_ended = (notes_judged >= chart->metrics.note_count);
	sample_progress += 1;
	auto const progress_ns = samples_to_ns(sample_progress);
	for (auto [lane, progress]: views::zip(chart->lanes, lane_progress)) {
		if (progress.next_note >= lane.notes.size()) continue;
		auto const& note = lane.notes[progress.next_note];
		if (progress_ns >= note.timestamp) {
			if (note.type_is<Note::Simple>() || (note.type_is<Note::LN>() && progress_ns >= note.timestamp + note.params<Note::LN>().length)) {
				progress.next_note += 1;
				if (lane.playable) notes_judged += 1;
				if (note.type_is<Note::LN>()) {
					progress.ln_active = false;
					continue;
				}
			}
			if (chart->wav_slots[note.wav_slot].empty()) continue;
			if (note.type_is<Note::Simple>() || (note.type_is<Note::LN>() && !progress.ln_active)) {
				wav_slot_progress[note.wav_slot].playback_pos = 0;
				if (note.type_is<Note::LN>()) progress.ln_active = true;
			}
		}
	}

	for (auto [slot, progress]: views::zip(chart->wav_slots, wav_slot_progress)) {
		if (progress.playback_pos == WavSlotProgress::Stopped) continue;
		auto const result = slot[progress.playback_pos];
		progress.playback_pos += 1;
		if (progress.playback_pos >= slot.size())
			progress.playback_pos = WavSlotProgress::Stopped;
		func(result);
		chart_ended = false;
	}

	return chart_ended;
}

template<callable<void(Note const&, Chart::LaneType, float)> Func>
void Cursor::upcoming_notes(float max_units, Func&& func) const noexcept
{
	auto const beat_duration = duration_cast<nanoseconds>(duration<double>{60.0 / chart->bpm});
	auto const measure_duration = beat_duration * 4;
	auto const current_y = duration_cast<duration<double>>(samples_to_ns(sample_progress)) / measure_duration;
	for (auto [idx, lane, progress]: views::zip(views::iota(0zu), chart->lanes, lane_progress) | views::filter([](auto tuple) { return get<1>(tuple).playable; })) {
		for (auto const& note: span{lane.notes.begin() + progress.next_note, lane.notes.size() - progress.next_note}) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			func(note, static_cast<Chart::LaneType>(idx), distance);
		}
	}
}

void Cursor::restart() noexcept
{
	sample_progress = 0zu;
	notes_judged = 0zu;
	for (auto& lane: lane_progress) lane.restart();
	for (auto& slot: wav_slot_progress) slot.playback_pos = WavSlotProgress::Stopped;
}

Cursor::Cursor(Chart const& chart) noexcept:
	chart{chart.shared_from_this()}
{
	wav_slot_progress.resize(chart.wav_slots.size());
}

auto Cursor::samples_to_ns(usize samples) noexcept -> nanoseconds
{
	auto const sampling_rate = io::AudioCodec::sampling_rate;
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	auto const whole_seconds = samples / sampling_rate;
	auto const remainder = samples % sampling_rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

}
