/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/cursor.cppm:
Representation of a moment in chart's playback. Advances one sample a time while producing audio.
*/

module;
#include "macros/assert.hpp"

export module playnote.bms.cursor;

import playnote.preamble;
import playnote.dev.audio;
import playnote.bms.chart;

namespace playnote::bms {

export class Cursor {
public:
	explicit Cursor(Chart const& chart) noexcept;

	[[nodiscard]] auto get_chart() const noexcept -> Chart const& { return *chart; }

	void restart() noexcept;

	template<callable<void(dev::Sample)> Func>
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

Cursor::Cursor(Chart const& chart) noexcept:
	chart{chart.shared_from_this()}
{
	wav_slot_progress.resize(chart.wav_slots.size());
}

void Cursor::restart() noexcept
{
	sample_progress = 0zu;
	notes_judged = 0zu;
	for (auto& lane: lane_progress) lane.restart();
	for (auto& slot: wav_slot_progress) slot.playback_pos = WavSlotProgress::Stopped;
}

template<callable<void(dev::Sample)> Func>
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

auto Cursor::samples_to_ns(usize samples) noexcept -> nanoseconds
{
	auto const sampling_rate = dev::Audio::get_sampling_rate();
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	auto const whole_seconds = samples / sampling_rate;
	auto const remainder = samples % sampling_rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

}
