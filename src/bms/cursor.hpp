/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/cursor.hpp:
Representation of a moment in chart's playback.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "bms/chart.hpp"
#include "dev/audio.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

// A cursor allows for playing back an immutable Chart, tracking all state related to note progress
// and audio playback progress.
class Cursor {
public:
	static constexpr auto   PGreatWindow =  16'670'000ns;
	static constexpr auto    GreatWindow =  33'330'000ns;
	static constexpr auto     GoodWindow = 116'670'000ns;
	static constexpr auto      BadWindow = 250'000'000ns;
	static constexpr auto MashPoorWindow = 500'000'000ns;

	// An immediate player input to the cursor's current position.
	struct LaneInput {
		Chart::LaneType lane;
		bool state;
	};

	// Create a cursor for the given chart. The chart's lifetime will be extended by the cursor's.
	explicit Cursor(Chart const& chart, bool autoplay);

	// Return the chart the cursor is attached to.
	[[nodiscard]] auto get_chart() const -> Chart const& { return *chart; }

	// Return the current position of the cursor in samples.
	[[nodiscard]] auto get_progress() const -> usize { return sample_progress; }

	// Return the current position of the cursor in nanoseconds.
	[[nodiscard]] auto get_progress_ns() const -> nanoseconds { return dev::Audio::samples_to_ns(get_progress()); }

	// Return the number of playable notes that were already judged.
	[[nodiscard]] auto get_judged_notes() const -> usize { return notes_judged; }

	// Seek the cursor to the beginning of the chart.
	void restart();

	// Progress by one audio sample, calling the provided function once for every active
	// sound playback. inputs contains any inputs that should be processed at the current cursor
	// position. use_bb controls if the bounding box should be used to speed up the lookup.
	template<callable<void(dev::Sample)> Func>
	auto advance_one_sample(Func&& func = [](dev::Sample){}, span<LaneInput const> inputs = {},
		bool use_bb = true) -> bool;

	// Progress by the given number of samples, without generating any audio.
	void fast_forward(usize samples);

	// Call the provided function for every note less than max_units away from current position.
	template<callable<void(Note const&, Chart::LaneType, float)> Func>
	void upcoming_notes(float max_units, Func&& func) const;

private:
	struct LaneProgress {
		usize next_note; // Index of the earliest note that hasn't been judged yet
		usize active_slot; // Index of the WAV slot that will be triggred on player input
		bool ln_active; // Is it currently in the middle of an LN?
		void restart() { *this = {}; }
	};
	struct WavSlotProgress {
		static constexpr auto Stopped = -1zu; // Special value for stopped playback
		usize playback_pos = Stopped; // Samples played so far
	};
	shared_ptr<Chart const> chart;
	bool autoplay;
	usize sample_progress = 0zu;
	usize notes_judged = 0zu;
	array<LaneProgress, +Chart::LaneType::Size> lane_progress = {};
	vector<WavSlotProgress> wav_slot_progress;

	void trigger_lane_input(Chart::LaneType lane, bool state);
};

[[nodiscard]] inline auto get_bpm_section(nanoseconds timestamp, span<BPMChange const> bpm_changes) -> BPMChange const&
{
	auto bpm_section = find_last_if(bpm_changes, [&](auto const& bc) {
		return timestamp >= bc.position;
	});
	ASSERT(!bpm_section.empty());
	return *bpm_section.begin();
}

inline Cursor::Cursor(Chart const& chart, bool autoplay):
	chart{chart.shared_from_this()},
	autoplay{autoplay}
{
	wav_slot_progress.resize(chart.wav_slots.size());
	for (auto idx: irange(0zu, +Chart::LaneType::Size)) {
		auto const& lane = chart.lanes[idx];
		auto& progress = lane_progress[idx];
		if (lane.notes.empty()) continue;
		progress.active_slot = lane.notes[0].wav_slot;
	}
}

inline void Cursor::restart()
{
	sample_progress = 0zu;
	notes_judged = 0zu;
	for (auto& lane: lane_progress) lane.restart();
	for (auto& slot: wav_slot_progress) slot.playback_pos = WavSlotProgress::Stopped;
}

inline void Cursor::fast_forward(usize samples)
{
	for (auto const i: irange(0zu, samples)) advance_one_sample([](dev::Sample){});
}

inline void Cursor::trigger_lane_input(Chart::LaneType lane_type, bool state)
{
	auto const& lane = chart->lanes[+lane_type];
	auto& progress = lane_progress[+lane_type];

	// Judge next note
	if (state && progress.next_note < lane.notes.size()) {
		auto const& note = lane.notes[progress.next_note];
		progress.active_slot = note.wav_slot;
		if (!note.type_is<Note::LN>()) {
			if (lane.playable) notes_judged += 1;
			progress.next_note += 1;
		} else {
			progress.ln_active = true;
		}
	}

	if (!state && progress.ln_active && progress.next_note < lane.notes.size()) {
		auto const& note = lane.notes[progress.next_note];
		if (note.type_is<Note::LN>()) {
			if (lane.playable) notes_judged += 1;
			progress.next_note += 1;
			progress.ln_active = false;
		}
	}

	// Trigger associated sample
	if (state && lane.audible && !chart->wav_slots[progress.active_slot].empty())
		wav_slot_progress[progress.active_slot].playback_pos = 0;
}

template<callable<void(dev::Sample)> Func>
auto Cursor::advance_one_sample(Func&& func, span<LaneInput const> inputs, bool use_bb) -> bool
{
	auto chart_ended = (notes_judged >= chart->metrics.note_count);
	sample_progress += 1;
	auto const progress_ns = dev::Audio::samples_to_ns(sample_progress);
	ASSUME(chart->lanes.size() == lane_progress.size());

	// Trigger inputs if unplayable or autoplay
	for (auto idx: irange(0zu, chart->lanes.size())) {
		auto const& lane = chart->lanes[idx];
		auto& progress = lane_progress[idx];
		if (progress.next_note >= lane.notes.size()) continue;
		auto const& note = lane.notes[progress.next_note];
		if (autoplay || !lane.playable) {
			if (progress_ns >= note.timestamp && !progress.ln_active) {
				trigger_lane_input(Chart::LaneType{idx}, true);
				if (!note.type_is<Note::LN>())
					trigger_lane_input(Chart::LaneType{idx}, false);
			}
			if (note.type_is<Note::LN>() && progress_ns >= note.timestamp + note.params<Note::LN>().length)
				trigger_lane_input(Chart::LaneType{idx}, false);
		}
	}

	// Trigger manual inputs
	if (!autoplay && !inputs.empty()) {
		for (auto const& input: inputs)
			trigger_lane_input(input.lane, input.state);
	}

	// Detect missed notes
	for (auto idx: irange(0zu, chart->lanes.size())) {
		auto const& lane = chart->lanes[idx];
		if (!lane.playable) continue;
		auto& progress = lane_progress[idx];
		if (progress.next_note >= lane.notes.size()) continue;
		auto const& note = lane.notes[progress.next_note];
		if (progress_ns - note.timestamp > BadWindow) {
			progress.next_note += 1;
			notes_judged += 1;
			if (progress.next_note >= lane.notes.size())
				progress.active_slot = lane.notes[progress.next_note].wav_slot;
		}
	}

	// Function to progress audio playback of a sample in a slot
	auto play_slot = [&](vector<dev::Sample> const& slot, WavSlotProgress& progress) {
		if (progress.playback_pos == WavSlotProgress::Stopped) return;
		auto const result = slot[progress.playback_pos];
		progress.playback_pos += 1;
		if (progress.playback_pos >= slot.size())
			progress.playback_pos = WavSlotProgress::Stopped;
		func(result);
		chart_ended = false;
	};

	// Advance sample playback
	if (use_bb) {
		auto window_id = clamp<usize>(progress_ns / SlotBB::WindowSize, 0zu, chart->slot_bb.windows.size() - 1);
		auto const& window = chart->slot_bb.windows[window_id];
		for (auto slot_id: window) {
			auto const& slot = chart->wav_slots[slot_id];
			auto& progress = wav_slot_progress[slot_id];
			play_slot(slot, progress);
		}
	} else {
		ASSUME(chart->wav_slots.size() == wav_slot_progress.size());
		for (auto idx: irange(0zu, chart->wav_slots.size()))
			play_slot(chart->wav_slots[idx], wav_slot_progress[idx]);
	}

	return chart_ended;
}

template<callable<void(Note const&, Chart::LaneType, float)> Func>
void Cursor::upcoming_notes(float max_units, Func&& func) const
{
	auto const progress_timestamp = dev::Audio::samples_to_ns(sample_progress);
	auto const& bpm_section = get_bpm_section(progress_timestamp, chart->bpm_changes);
	auto const section_progress = progress_timestamp - bpm_section.position;
	auto const beat_duration = duration<double>{60.0 / chart->bpm};
	auto const bpm_ratio = bpm_section.bpm / chart->bpm_changes[0].bpm;
	auto const current_y = bpm_section.y_pos + section_progress / beat_duration * bpm_ratio * bpm_section.scroll_speed;
	ASSUME(chart->lanes.size() == lane_progress.size());
	for (auto idx: irange(0zu, chart->lanes.size())) {
		auto const& lane = chart->lanes[idx];
		if (!lane.visible) continue;
		auto& progress = lane_progress[idx];
		for (auto const& note: span{lane.notes.begin() + progress.next_note, lane.notes.size() - progress.next_note}) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			func(note, static_cast<Chart::LaneType>(idx), distance);
		}
	}
}

}
