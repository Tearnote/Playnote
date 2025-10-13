/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/cursor.hpp:
A tracker of Chart playback. Given user input events, it advances chart progress and generates audio playback events
and note hit events.
*/

#pragma once
#include "preamble.hpp"
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
		usize channel; // BMS channel index; the event should interrupt playback of any samples on the same channel
		span<dev::Sample const> audio;
	};

	// Create a cursor for the given chart.
	explicit Cursor(shared_ptr<Chart const> chart, bool autoplay = false);

	// Return the chart the cursor is attached to.
	[[nodiscard]] auto get_chart() const -> Chart const& { return *chart; }

	// Return the current position of the cursor in samples.
	[[nodiscard]] auto get_progress() const -> usize { return sample_progress; }

	// Return the current position of the cursor in nanoseconds.
	[[nodiscard]] auto get_progress_ns() const -> nanoseconds { return globals::mixer->get_audio().samples_to_ns(get_progress()); }

	// true if a lane is currently being held, false otherwise.
	[[nodiscard]] auto is_pressed(Lane::Type lane) const -> bool { return lane_progress[+lane].pressed; }

	// Call the provided function for every pending judgment event.
	template<callable<void(JudgmentEvent&&)> Func>
	void each_judgment_event(Func&& func);

	// Progress by one audio sample, calling the provided function once for every newly started sound.
	// Can be optionally provided with inputs that will affect chart playback (ignored if autoplay).
	// Returns false if the chart has ended. In this state no more new audio will be triggered,
	// and the fate of all notes has been determined.
	template<callable<void(SoundEvent)> Func>
	auto advance_one_sample(Func&& func, span<LaneInput const> inputs = {}) -> bool;

	// Progress by the given number of samples, without generating any audio.
	void fast_forward(usize samples);

	// Call the provided function for every note less than max_units away from current position.
	template<callable<void(Note const&, Lane::Type, float)> Func>
	void upcoming_notes(float max_units, Func&& func, nanoseconds offset = 0ns, bool adjust_for_latency = false) const;

	Cursor(Cursor const& other) { *this = other; }
	auto operator=(Cursor const&) -> Cursor&;

private:
	struct LaneProgress {
		usize next_note; // Index of the earliest note that hasn't been judged yet
		usize active_slot; // Index of the WAV slot that will be triggered on player input
		bool pressed; // Is the player currently pushing the lane's button?
		optional<nanoseconds> ln_timing; // If an LN is being held, the value is the timing of the LN's start's hit
	};

	shared_ptr<Chart const> chart;
	bool autoplay;
	usize sample_progress = 0;
	array<LaneProgress, enum_count<Lane::Type>()> lane_progress = {};
	spsc_queue<JudgmentEvent> judgment_events;

	template<callable<void(SoundEvent)> Func>
	void trigger_input(LaneInput, Func&&);
	void trigger_miss(Lane::Type);
	void trigger_ln_release(Lane::Type);
	auto get_bpm_section(nanoseconds timestamp) const -> BPMChange const&;
};

inline Cursor::Cursor(shared_ptr<Chart const> chart, bool autoplay):
	chart{move(chart)},
	autoplay{autoplay}
{
	// Initialize lanes to play the first upcoming WAV slot
	for (auto [lane, progress]: views::zip(this->chart->timeline.lanes, lane_progress)) {
		if (lane.notes.empty()) continue;
		progress.active_slot = lane.notes[0].wav_slot;
	}
}

template<callable<void(Cursor::JudgmentEvent&&)> Func>
void Cursor::each_judgment_event(Func&& func)
{
	auto event = JudgmentEvent{};
	while (judgment_events.try_dequeue(event))
		func(move(event));
}

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
			if (progress.next_note >= lane.notes.size()) continue;
			Note const& note = lane.notes[progress.next_note];
			if (lane.playable && get_progress_ns() - note.timestamp > HitWindow && !progress.ln_timing)
				trigger_miss(type);
		}

		// Autoplay and unplayable inputs
		if (autoplay || !lane.playable) {
			if (progress.next_note >= lane.notes.size()) continue;
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
			if (progress.next_note >= lane.notes.size()) continue;
			Note const& note = lane.notes[progress.next_note];
			if (lane.playable && note.type_is<Note::LN>() && note.timestamp + note.params<Note::LN>().length <= get_progress_ns())
				trigger_ln_release(type);
		}
	}

	sample_progress += 1;
	return get_progress_ns() < chart->metadata.chart_duration;
}

template<callable<void(Note const&, Lane::Type, float)> Func>
void Cursor::upcoming_notes(float max_units, Func&& func, nanoseconds offset, bool adjust_for_latency) const
{
	auto const latency_adjustment = adjust_for_latency? -globals::mixer->get_latency() : 0ns;
	auto const progress_timestamp = globals::mixer->get_audio().samples_to_ns(sample_progress) + latency_adjustment - offset;
	auto const& bpm_section = get_bpm_section(progress_timestamp);
	auto const section_progress = progress_timestamp - bpm_section.position;
	auto const beat_duration = duration<double>{60.0 / chart->metadata.bpm_range.main};
	auto const bpm_ratio = bpm_section.bpm / chart->timeline.bpm_sections[0].bpm;
	auto const current_y = bpm_section.y_pos + section_progress / beat_duration * bpm_ratio * bpm_section.scroll_speed;
	for (auto [idx, lane, progress]: views::zip(views::iota(0zu), chart->timeline.lanes, lane_progress)) {
		if (!lane.visible) continue;
		for (auto const& note: span{lane.notes.begin() + progress.next_note, lane.notes.size() - progress.next_note}) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			func(note, static_cast<Lane::Type>(idx), distance);
		}
	}
}

inline void Cursor::fast_forward(usize samples)
{
	for (auto const _: views::iota(0zu, samples)) advance_one_sample([](SoundEvent){});
}

inline auto Cursor::operator=(Cursor const& other) -> Cursor&
{
	chart = other.chart;
	autoplay = other.autoplay;
	sample_progress = other.sample_progress;
	lane_progress = other.lane_progress;
	return *this;
}

template<callable<void(Cursor::SoundEvent)> Func>
void Cursor::trigger_input(LaneInput input, Func&& func)
{
	auto& progress = lane_progress[+input.lane];
	auto const& lane = chart->timeline.lanes[+input.lane];

	if (progress.pressed == input.state) return;

	// Judge input against current/next note
	if (progress.next_note < lane.notes.size()) {
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
				if (lane.audible && note.wav_slot != -1u) {
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
				if (lane.audible && note.wav_slot != -1u) {
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
			if (lane.audible && progress.active_slot != -1u) {
				func(SoundEvent{
					.channel = progress.active_slot,
					.audio = chart->media.wav_slots[progress.active_slot],
				});
			}
		}
	}

	progress.pressed = input.state;
}

inline void Cursor::trigger_miss(Lane::Type type)
{
	auto& progress = lane_progress[+type];
	auto const& lane = chart->timeline.lanes[+type];
	auto const& note = lane.notes[progress.next_note];

	if (lane.playable) {
		judgment_events.enqueue(JudgmentEvent{
			.type = note.type_is<Note::Simple>()? JudgmentEvent::Type::Note : JudgmentEvent::Type::LN,
			.lane = type,
		});
	}
	progress.active_slot = note.wav_slot;
	progress.next_note += 1;
}

inline void Cursor::trigger_ln_release(Lane::Type type)
{
	auto& progress = lane_progress[+type];
	auto const& lane = chart->timeline.lanes[+type];
	auto const& note = lane.notes[progress.next_note];

	if (lane.playable) {
		judgment_events.enqueue(JudgmentEvent{
			.type = JudgmentEvent::Type::LN,
			.lane = type,
			.timing = *progress.ln_timing,
			.release_timing = get_progress_ns() - (note.timestamp + note.params<Note::LN>().length),
		});
	}
	progress.ln_timing = nullopt;
	progress.next_note += 1;
}

inline auto Cursor::get_bpm_section(nanoseconds timestamp) const -> BPMChange const&
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
