/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "bms/cursor.hpp"

#include "preamble.hpp"
#include "utils/assert.hpp"

namespace playnote::bms {

Cursor::Cursor(shared_ptr<Chart const> chart, bool autoplay):
	chart{move(chart)},
	autoplay{autoplay}
{
	// Initialize lanes to play the first upcoming WAV slot
	for (auto [lane, progress]: views::zip(this->chart->timeline.lanes, lane_progress)) {
		if (lane.notes.empty()) continue;
		progress.active_slot = lane.notes[0].wav_slot;
	}
}

auto Cursor::pending_judgment_events() -> generator<JudgmentEvent>
{
	auto event = JudgmentEvent{};
	while (judgment_events.try_dequeue(event)) co_yield move(event);
}

void Cursor::seek(ssize_t sample_position)
{
	sample_progress = sample_position;
	auto const progress_ns = get_progress_ns();
	for (auto [progress, lane]: views::zip(lane_progress, chart->timeline.lanes)) {
		auto first_unplayed_note = find_if(lane.notes, [&](Note const& note) {
			auto timestamp = note.timestamp;
			if (note.type_is<Note::LN>()) timestamp += note.params<Note::LN>().length;
			return timestamp > progress_ns;
		});
		progress.next_note = distance(lane.notes.begin(), first_unplayed_note);
		progress.ln_timing = nullopt;
		progress.pressed = false;
		if (first_unplayed_note == lane.notes.end()) {
			progress.active_slot = lane.notes.empty()? -1 : lane.notes.back().wav_slot;
			continue;
		}

		auto const& next_note = lane.notes[progress.next_note];
		progress.active_slot = next_note.wav_slot;
		if (next_note.type_is<Note::LN>() && next_note.timestamp <= progress_ns) {
			progress.ln_timing = 0ns;
			progress.pressed = true;
		}
	}
}

void Cursor::seek_relative(ssize_t sample_offset)
{
	if (sample_offset < 0 || autoplay) {
		seek(sample_progress + sample_offset);
		return;
	}
	for (auto _: views::iota(0z, sample_offset)) advance_one_sample([](auto){});
}

auto Cursor::upcoming_notes(float max_units, nanoseconds offset, bool adjust_for_latency) const -> generator<UpcomingNote>
{
	auto const latency_adjustment = adjust_for_latency? -globals::mixer->get_latency() : 0ns;
	auto const progress_timestamp = globals::mixer->get_audio().samples_to_ns(sample_progress, chart->media.sampling_rate) + latency_adjustment - offset;
	auto const& bpm_section = get_bpm_section(progress_timestamp);
	auto const section_progress = progress_timestamp - bpm_section.position;
	auto const beat_duration = duration<double>{60.0 / chart->metadata.bpm_range.main};
	auto const bpm_ratio = bpm_section.bpm / chart->timeline.bpm_sections[0].bpm;
	auto const current_y = bpm_section.y_pos + section_progress / beat_duration * bpm_ratio * bpm_section.scroll_speed;
	for (auto [idx, lane, progress]: views::zip(views::iota(0z), chart->timeline.lanes, lane_progress)) {
		if (!lane.visible) continue;
		for (auto [note_idx, note]: views::zip(
			views::iota(progress.next_note),
			span{lane.notes.begin() + progress.next_note, lane.notes.size() - progress.next_note}
		)) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			co_yield UpcomingNote{
				.note = note,
				.lane = static_cast<Lane::Type>(idx),
				.lane_idx = note_idx,
				.distance = static_cast<float>(distance),
			};
		}
	}
}

auto Cursor::operator=(Cursor const& other) -> Cursor&
{
	chart = other.chart;
	autoplay = other.autoplay;
	sample_progress = other.sample_progress;
	lane_progress = other.lane_progress;
	return *this;
}

void Cursor::trigger_miss(Lane::Type type)
{
	auto& progress = lane_progress[+type];
	auto const& lane = chart->timeline.lanes[+type];
	auto const& note = lane.notes[progress.next_note];

	if (lane.playable) {
		judgment_events.enqueue(JudgmentEvent{
			.type = note.type_is<Note::Simple>()? JudgmentEvent::Type::Note : JudgmentEvent::Type::LN,
			.lane = type,
			.timestamp = get_progress_ns(),
		});
	}
	progress.active_slot = note.wav_slot;
	progress.next_note += 1;
}

void Cursor::trigger_ln_release(Lane::Type type)
{
	auto& progress = lane_progress[+type];
	auto const& lane = chart->timeline.lanes[+type];
	auto const& note = lane.notes[progress.next_note];

	if (lane.playable) {
		judgment_events.enqueue(JudgmentEvent{
			.type = JudgmentEvent::Type::LN,
			.lane = type,
			.timestamp = get_progress_ns(),
			.timing = *progress.ln_timing,
			.release_timing = get_progress_ns() - (note.timestamp + note.params<Note::LN>().length),
		});
	}
	progress.ln_timing = nullopt;
	progress.next_note += 1;
}

auto Cursor::get_bpm_section(nanoseconds timestamp) const -> BPMChange const&
{
	auto const& bpm_sections = chart->timeline.bpm_sections;
	if (timestamp < 0ns) return bpm_sections[0];
	auto bpm_section = find_last_if(bpm_sections, [&](auto const& bc) {
		return timestamp >= bc.position;
	});
	ASSERT(!bpm_section.empty());
	return *bpm_section.begin();
}

}
