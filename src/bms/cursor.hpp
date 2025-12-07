/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "dev/audio.hpp"
#include "audio/mixer.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

class Cursor {
public:
	// Time window during which a note can be hit. No judgment and no impact on note progress happens outside of this.
	static constexpr auto HitWindow = 240ms;

	// An immediate player input to the cursor's current position. Subset of bms::Input.
	struct LaneInput {
		Lane::Type lane;
		bool state;
	};

	// A note hit event that can be judged.
	struct JudgmentEvent {
		enum class Type {
			Note, // An input was made against a note
			LNStart, // An LN start was hit. The judgment won't be finalized until the LN ends; this is only for display purposes
			LN, // An LN was completed in some way
		};
		Type type;
		Lane::Type lane;
		nanoseconds timestamp;
		// In the fields below, positive value means the player was late, negative means the player was early.
		optional<nanoseconds> timing; // nullopt: note was missed entirely. Note: timing of the note hit. LNStart, LN: timing of LN start
		optional<nanoseconds> release_timing; // Note, LNStart: irrelevant, nullopt. LN: timing of the release against LN end
	};

	// An audio playback trigger event.
	struct SoundEvent {
		isize_t channel; // BMS channel index; the event should interrupt playback of any samples on the same channel
		span<dev::Sample const> audio;
	};

	// Create a cursor for the given chart.
	explicit Cursor(shared_ptr<Chart const> chart, bool autoplay = false);

	// Return the chart the cursor is attached to.
	[[nodiscard]] auto get_chart() const -> Chart const& { return *chart; }

	// Return the current position of the cursor in samples.
	[[nodiscard]] auto get_progress() const -> isize_t { return sample_progress; }

	// Return the current position of the cursor in nanoseconds.
	[[nodiscard]] auto get_progress_ns() const -> nanoseconds { return globals::mixer->get_audio().samples_to_ns(get_progress(), chart->media.sampling_rate); }

	// true if a lane is currently being held, false otherwise.
	[[nodiscard]] auto is_pressed(Lane::Type lane) const -> bool { return lane_progress[+lane].pressed; }

	// Return every judgment event since the last time this was called.
	auto pending_judgment_events() -> generator<JudgmentEvent>;

	// Progress by one audio sample, calling the provided function once for every newly started sound.
	// Can be optionally provided with inputs that will affect chart playback (ignored if autoplay).
	// Returns false if the chart has ended. In this state no more new audio will be triggered,
	// and the fate of all notes has been determined.
	template<callable<void(SoundEvent)> Func>
	auto advance_one_sample(Func&& func, span<LaneInput const> inputs = {}) -> bool;

	// Directly modify current position, without triggering any audio or judgment events in between
	// current position and the destination. Note progress will update for the new position, as if
	// the chart has been autoplayed up to this point. Input queue is unaffected; you might want to
	// flush it.
	void seek(isize_t sample_position);

	// Seek to a specified timestamp. The same precautions apply as for seek().
	void seek_ns(nanoseconds timestamp) { seek(globals::mixer->get_audio().ns_to_samples(timestamp, chart->media.sampling_rate)); }

	// Seek relative to current position. If seek is backward, or autoplay is on, lane progress is
	// driven automatically. Otherwise, functions as a fast-forward.
	void seek_relative(isize_t sample_offset);

	// Return every note less than max_units away from current position.
	struct UpcomingNote {
		Note const& note;
		Lane::Type lane;
		isize_t lane_idx;
		float distance; // From current chart position, in units
	};
	auto upcoming_notes(float max_units, nanoseconds offset = 0ns, bool adjust_for_latency = false) const -> generator<UpcomingNote>;

	// For a given lane, return the index of the next note to be judged. Every note with a smaller
	// index has already been judged and should not be visible to the player.
	auto next_note_idx(Lane::Type lane) const -> isize_t { return lane_progress[+lane].next_note; }

	Cursor(Cursor const& other) { *this = other; }
	auto operator=(Cursor const&) -> Cursor&;

private:
	struct LaneProgress {
		isize_t next_note; // Index of the earliest note that hasn't been judged yet
		isize_t active_slot; // Index of the WAV slot that will be triggered on player input
		bool pressed; // Is the player currently pushing the lane's button?
		optional<nanoseconds> ln_timing; // If an LN is being held, the value is the timing of the LN's start's hit
	};

	shared_ptr<Chart const> chart;
	bool autoplay;
	isize_t sample_progress = 0;
	array<LaneProgress, enum_count<Lane::Type>()> lane_progress = {};
	spsc_queue<JudgmentEvent> judgment_events;

	template<callable<void(SoundEvent)> Func>
	void trigger_input(LaneInput, Func&&);
	void trigger_miss(Lane::Type);
	void trigger_ln_release(Lane::Type);
	auto get_bpm_section(nanoseconds timestamp) const -> BPMChange const&;
};

template<callable<void(Cursor::SoundEvent)> Func>
auto Cursor::advance_one_sample(Func&& func, span<LaneInput const> inputs) -> bool
{
	// Manual inputs
	if (!autoplay) {
		for (auto const& input: inputs)
			trigger_input(input, func);
	}

	for (auto [type, lane, progress]: views::zip(
		views::iota(0u) | views::transform([](auto i) { return static_cast<Lane::Type>(i); }),
		chart->timeline.lanes,lane_progress))
	{
		// Missed notes
		{
			if (progress.next_note >= static_cast<isize_t>(lane.notes.size())) continue;
			Note const& note = lane.notes[progress.next_note];
			if (lane.playable && get_progress_ns() - note.timestamp > HitWindow && !progress.ln_timing)
				trigger_miss(type);
		}

		// Autoplay and unplayable inputs
		if (autoplay || !lane.playable) {
			if (progress.next_note >= static_cast<isize_t>(lane.notes.size())) continue;
			Note const& note = lane.notes[progress.next_note];

			// The note just started
			if (get_progress_ns() >= note.timestamp && !progress.ln_timing) {
				trigger_input({type, true}, func);
				if (!note.type_is<Note::LN>())
					trigger_input({type, false}, func);
			}
			// The LN just ended
			if (note.type_is<Note::LN>() && note.timestamp + note.params<Note::LN>().length <= get_progress_ns())
				trigger_input({type, false}, func);
		}

		// Auto-completed LNs
		{
			if (progress.next_note >= static_cast<isize_t>(lane.notes.size())) continue;
			Note const& note = lane.notes[progress.next_note];
			if (lane.playable && note.type_is<Note::LN>() && note.timestamp + note.params<Note::LN>().length <= get_progress_ns())
				trigger_ln_release(type);
		}
	}

	sample_progress += 1;
	return get_progress_ns() < chart->metadata.chart_duration;
}

template<callable<void(Cursor::SoundEvent)> Func>
void Cursor::trigger_input(LaneInput input, Func&& func)
{
	auto& progress = lane_progress[+input.lane];
	auto const& lane = chart->timeline.lanes[+input.lane];

	if (progress.pressed == input.state) return;

	// Judge input against current/next note
	if (progress.next_note < static_cast<isize_t>(lane.notes.size())) {
		auto const& note = lane.notes[progress.next_note];

		if (input.state) {
			if (note.timestamp - get_progress_ns() <= HitWindow) {
				// A note is in range and was hit
				if (lane.playable) {
					judgment_events.enqueue(JudgmentEvent{
						.type = note.type_is<Note::Simple>()? JudgmentEvent::Type::Note : JudgmentEvent::Type::LNStart,
						.lane = input.lane,
						.timestamp = get_progress_ns(),
						.timing = get_progress_ns() - note.timestamp,
					});
				}
				if (lane.audible && note.wav_slot != -1 && !chart->media.wav_slots[note.wav_slot].empty()) {
					func(SoundEvent{
						.channel = note.wav_slot,
						.audio = chart->media.wav_slots[note.wav_slot],
					});
				}

				progress.active_slot = note.wav_slot;
				if (note.type_is<Note::Simple>())
					progress.next_note += 1;
				else
					progress.ln_timing = get_progress_ns() - note.timestamp;
			} else {
				// Press is too early to affect the note
				if (lane.audible && note.wav_slot != -1 && !chart->media.wav_slots[note.wav_slot].empty()) {
					func(SoundEvent{
						.channel = progress.active_slot,
						.audio = chart->media.wav_slots[progress.active_slot],
					});
				}
			}
		}

		// LN was released early
		if (!input.state && progress.ln_timing)
			trigger_ln_release(input.lane);
	} else {
		if (input.state) {
			// Chart over, player is just pressing things for fun
			if (lane.audible && progress.active_slot != -1 && !chart->media.wav_slots[progress.active_slot].empty()) {
				func(SoundEvent{
					.channel = progress.active_slot,
					.audio = chart->media.wav_slots[progress.active_slot],
				});
			}
		}
	}

	progress.pressed = input.state;
}

}
