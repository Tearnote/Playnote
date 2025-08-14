/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/cursor.hpp:
Representation of a moment in chart's playback.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "dev/audio.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

// A cursor allows for playing back an immutable Chart, tracking all state related to note progress
// and audio playback progress.
class Cursor {
public:
	// Create a cursor for the given chart. The chart's lifetime will be extended by the cursor's.
	explicit Cursor(Chart const& chart);

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
	// sound playback.
	template<callable<void(dev::Sample)> Func>
	auto advance_one_sample(Func&& func = [](dev::Sample){}, bool use_bb = true) -> bool;

	// Progress by the given number of samples, without generating any audio.
	void fast_forward(usize samples);

	// Call the provided function for every note less than max_units away from current position.
	template<callable<void(Note const&, Chart::LaneType, float)> Func>
	void upcoming_notes(float max_units, Func&& func) const;

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
};

[[nodiscard]] inline auto get_bpm_section(nanoseconds timestamp, span<BPMChange const> bpm_changes) -> BPMChange const&
{
	auto bpm_section = bpm_changes | views::reverse | views::filter([&](auto const& bc) {
		if (timestamp >= bc.position) return true;
		return false;
	}) | views::take(1);
	ASSERT(!bpm_section.empty());
	return *bpm_section.begin();
}

inline Cursor::Cursor(Chart const& chart):
	chart{chart.shared_from_this()}
{
	wav_slot_progress.resize(chart.wav_slots.size());
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
	for (auto const i: views::iota(0zu, samples)) advance_one_sample([](dev::Sample){});
}

template<callable<void(dev::Sample)> Func>
auto Cursor::advance_one_sample(Func&& func, bool use_bb) -> bool
{
	auto chart_ended = (notes_judged >= chart->metrics.note_count);
	sample_progress += 1;
	auto const progress_ns = dev::Audio::samples_to_ns(sample_progress);
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
			if (!lane.audible) continue; // We still mark the note triggered, but that's it
			if (chart->wav_slots[note.wav_slot].empty()) continue;
			if (note.type_is<Note::Simple>() || (note.type_is<Note::LN>() && !progress.ln_active)) {
				wav_slot_progress[note.wav_slot].playback_pos = 0;
				if (note.type_is<Note::LN>()) progress.ln_active = true;
			}
		}
	}

	auto play_slot = [&](vector<dev::Sample> const& slot, WavSlotProgress& progress) {
		if (progress.playback_pos == WavSlotProgress::Stopped) return;
		auto const result = slot[progress.playback_pos];
		progress.playback_pos += 1;
		if (progress.playback_pos >= slot.size())
			progress.playback_pos = WavSlotProgress::Stopped;
		func(result);
		chart_ended = false;
	};

	if (use_bb) {
		auto window_id = clamp<usize>(progress_ns / SlotBB::WindowSize, 0zu, chart->slot_bb.windows.size() - 1);
		auto const& window = chart->slot_bb.windows[window_id];
		for (auto slot_id: window) {
			auto const& slot = chart->wav_slots[slot_id];
			auto& progress = wav_slot_progress[slot_id];
			play_slot(slot, progress);
		}
	} else {
		for (auto [slot, progress]: views::zip(chart->wav_slots, wav_slot_progress)) {
			play_slot(slot, progress);
		}
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
	for (auto [idx, lane, progress]: views::zip(views::iota(0zu), chart->lanes, lane_progress) | views::filter([](auto const& tuple) { return get<1>(tuple).visible; })) {
		for (auto const& note: span{lane.notes.begin() + progress.next_note, lane.notes.size() - progress.next_note}) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			func(note, static_cast<Chart::LaneType>(idx), distance);
		}
	}
}

}
