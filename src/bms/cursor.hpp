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
#include "audio/mixer.hpp"

namespace playnote::bms {

// A cursor allows for playing back an immutable Chart, tracking all state related to note progress
// and audio playback progress.
class Cursor {
public:
	static constexpr auto   PGreatWindow =  18ms;
	static constexpr auto    GreatWindow =  36ms;
	static constexpr auto     GoodWindow = 120ms;
	static constexpr auto      BadWindow = 240ms;
	static constexpr auto MashPoorWindow = 500ms;
	static constexpr auto LNEarlyRelease = 120ms; // How early can an LN be released and still count as a PGreat

	enum class Rank {
		AAA,
		AA,
		A,
		B,
		C,
		D,
		E,
		F,
	};

	// An immediate player input to the cursor's current position.
	struct LaneInput {
		Lane::Type lane;
		bool state;
	};

	struct Judgment {
		enum class Type {
			PGreat,
			Great,
			Good,
			Bad,
			Poor,
		};
		enum class Timing {
			None, // For miss/release
			Early,
			OnTime,
			Late,
		};
		Type type;
		Timing timing;
		nanoseconds timestamp;
	};

	// Totals of each judgment type.
	struct JudgeTotals {
		array<usize, enum_count<Judgment::Type>()> types;
		array<usize, enum_count<Judgment::Timing>()> timings;
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

	// Return the note judgments.
	[[nodiscard]] auto get_judge_totals() const -> JudgeTotals { return judge_totals; }

	// Return the latest judgment on given playfield.
	[[nodiscard]] auto get_latest_judgment(uint32 field_idx) const -> optional<Judgment>
	{
		return latest_judgement[field_idx];
	}

	// Return current combo.
	[[nodiscard]] auto get_combo() const -> usize { return combo; }

	// Return current score.
	[[nodiscard]] auto get_score() const -> usize { return score; }

	// Return accuracy rank.
	[[nodiscard]] auto get_rank() const -> Rank;

	// true if a lane is currently being held, false otherwise.
	[[nodiscard]] auto is_pressed(Lane::Type) const -> bool;

	// Seek the cursor to the beginning of the chart.
	void restart();

	// Progress by one audio sample, calling the provided function once for every active
	// sound playback. inputs contains any inputs that should be processed at the current cursor
	// position. use_bb controls if the bounding box should be used to speed up the lookup.
	template<callable<void(dev::Sample)> Func>
	auto advance_one_sample(Func&& func = [](dev::Sample){}, span<LaneInput const> inputs = {}) -> bool;

	// Progress by the given number of samples, without generating any audio.
	void fast_forward(usize samples);

	// Call the provided function for every note less than max_units away from current position.
	template<callable<void(Note const&, Lane::Type, float)> Func>
	void upcoming_notes(float max_units, Func&& func, nanoseconds offset = 0ns, bool adjust_for_latency = false) const;

private:
	struct LaneProgress {
		usize next_note; // Index of the earliest note that hasn't been judged yet
		usize active_slot; // Index of the WAV slot that will be triggered on player input
		bool pressed; // Is the player currently pushing the lane's button?
		bool ln_active; // Is it currently in the middle of an LN?
		void restart() { *this = {}; }
	};
	struct WavSlotProgress {
		static constexpr auto Stopped = -1zu; // Special value for stopped playback
		usize playback_pos = Stopped; // Samples played so far
	};
	shared_ptr<Chart const> chart;
	bool autoplay;
	usize sample_progress;
	usize notes_judged;
	array<LaneProgress, enum_count<Lane::Type>()> lane_progress = {};
	vector<WavSlotProgress> wav_slot_progress;
	JudgeTotals judge_totals;
	array<optional<Judgment>, 2> latest_judgement = {};
	usize combo;
	usize score;

	void trigger_lane_input(Lane const&, Lane::Type, LaneProgress&, bool state);
	void apply_judgment(Judgment, Lane::Type);
};

[[nodiscard]] inline auto get_bpm_section(nanoseconds timestamp, span<BPMChange const> bpm_changes) -> BPMChange const&
{
	if (timestamp < 0ns) return bpm_changes[0];
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
	wav_slot_progress.resize(chart.media.wav_slots.size());
	restart();
}

inline auto Cursor::get_rank() const -> Rank
{
	if (notes_judged == 0) return Rank::AAA;
	auto const acc = static_cast<double>(score) / static_cast<double>(notes_judged * 2);
	if (acc >= 8.0 / 9.0) return Rank::AAA;
	if (acc >= 7.0 / 9.0) return Rank::AA;
	if (acc >= 6.0 / 9.0) return Rank::A;
	if (acc >= 5.0 / 9.0) return Rank::B;
	if (acc >= 4.0 / 9.0) return Rank::C;
	if (acc >= 3.0 / 9.0) return Rank::D;
	if (acc >= 2.0 / 9.0) return Rank::E;
	return Rank::F;
}

inline auto Cursor::is_pressed(Lane::Type type) const -> bool
{
	return lane_progress[+type].pressed;
}

inline void Cursor::restart()
{
	sample_progress = 0zu;
	notes_judged = 0zu;
	for (auto& lane: lane_progress) lane.restart();
	for (auto& slot: wav_slot_progress) slot.playback_pos = WavSlotProgress::Stopped;
	judge_totals = {};
	combo = 0;
	score = 0;

	for (auto [lane, progress]: views::zip(chart->timeline.lanes, lane_progress)) {
		if (lane.notes.empty()) continue;
		progress.active_slot = lane.notes[0].wav_slot;
	}
}

inline void Cursor::fast_forward(usize samples)
{
	for (auto const i: views::iota(0zu, samples)) advance_one_sample([](dev::Sample){});
}

inline void Cursor::trigger_lane_input(Lane const& lane, Lane::Type type, LaneProgress& progress, bool state)
{
	if (progress.pressed == state) return;

	// Judge press
	if (state && progress.next_note < lane.notes.size()) {
		auto const& note = lane.notes[progress.next_note];
		auto const timing_ns = note.timestamp - get_progress_ns(); // +: in the future, -: in the past

		if (timing_ns <= MashPoorWindow) {
			if (timing_ns > BadWindow && lane.playable) {
				// Mashpoor
				apply_judgment({Judgment::Type::Poor}, type);
			} else {
				// The note will now be removed by any judgment
				auto const abs_timing_ns = abs(timing_ns);
				if (lane.playable) {
					if (abs_timing_ns <= PGreatWindow) {
						apply_judgment({Judgment::Type::PGreat, Judgment::Timing::OnTime}, type);
					} else {
						auto const timing = timing_ns < 0ns? Judgment::Timing::Late : Judgment::Timing::Early;
						if (abs_timing_ns <= GreatWindow)
							apply_judgment({Judgment::Type::Great, timing}, type);
						else if (abs_timing_ns <= GoodWindow)
							apply_judgment({Judgment::Type::Good, timing}, type);
						else
							apply_judgment({Judgment::Type::Bad, timing}, type);
					}
				}
				if (!note.type_is<Note::LN>()) {
					if (lane.playable) notes_judged += 1;
					progress.next_note += 1;
				} else {
					progress.ln_active = true;
				}
				progress.active_slot = note.wav_slot;
			}
		}

	}

	// Judge release
	if (!state && progress.ln_active && progress.next_note < lane.notes.size()) {
		auto const& note = lane.notes[progress.next_note];
		if (note.type_is<Note::LN>()) {
			if (lane.playable) {
				if (note.timestamp + note.params<Note::LN>().length > get_progress_ns()) {
					judge_totals.types[+Judgment::Type::Poor] += 1;
					combo = 0;
				} else {
					judge_totals.types[+Judgment::Type::PGreat] += 1;
					combo += 1;
				}
			}
			if (lane.playable) notes_judged += 1;
			progress.next_note += 1;
			progress.ln_active = false;
		}
	}

	// Trigger associated sample
	if (state && lane.audible && !chart->media.wav_slots[progress.active_slot].empty())
		wav_slot_progress[progress.active_slot].playback_pos = 0;

	progress.pressed = state;
}

inline void Cursor::apply_judgment(Judgment judgment, Lane::Type lane)
{
	judgment.timestamp = get_progress_ns();
	judge_totals.types[+judgment.type] += 1;
	judge_totals.timings[+judgment.timing] += 1;

	switch (judgment.type) {
	case Judgment::Type::PGreat:
		combo += 1;
		score += 2;
		break;
	case Judgment::Type::Great:
		combo += 1;
		score += 1;
		break;
	case Judgment::Type::Good:
		combo += 1;
		break;
	case Judgment::Type::Bad:
	case Judgment::Type::Poor:
		combo = 0;
		break;
	}

	auto const field_id = +lane < +Lane::Type::P2_Key1? 0 : 1;
	latest_judgement[field_id] = judgment;
}

template<callable<void(dev::Sample)> Func>
auto Cursor::advance_one_sample(Func&& func, span<LaneInput const> inputs) -> bool
{
	auto chart_ended = (notes_judged >= chart->metadata.note_count);
	sample_progress += 1;
	auto const progress_ns = dev::Audio::samples_to_ns(sample_progress);

	// Trigger inputs if unplayable or autoplay
	for (auto [type, lane, progress]: views::zip(
		views::iota(0u) | views::transform([](auto i) { return static_cast<Lane::Type>(i); }),
		chart->timeline.lanes,
		lane_progress
		) | views::filter([](auto const& view) { return get<2>(view).next_note < get<1>(view).notes.size(); })) {
		Note const& note = lane.notes[progress.next_note];
		if (autoplay || !lane.playable) {
			if (progress_ns >= note.timestamp && !progress.ln_active) {
				trigger_lane_input(lane, type, progress, true);
				if (!note.type_is<Note::LN>())
					trigger_lane_input(lane, type, progress, false);
			}
			if (note.type_is<Note::LN>() && progress_ns >= note.timestamp + note.params<Note::LN>().length)
				trigger_lane_input(lane, type, progress, false);
		}
	}

	// Trigger manual inputs
	if (!autoplay && !inputs.empty()) {
		for (auto const& input: inputs)
			trigger_lane_input(chart->timeline.lanes[+input.lane], input.lane, lane_progress[+input.lane], input.state);
	}

	// Detect missed notes
	for (auto [type, lane, progress]: views::zip(
		views::iota(0u) | views::transform([](auto i) { return static_cast<Lane::Type>(i); }),
		chart->timeline.lanes,
		lane_progress
		) | views::filter([](auto const& view) { return get<1>(view).playable; })) {
		if (progress.next_note >= lane.notes.size()) continue;
		auto const& note = lane.notes[progress.next_note];
		if (progress_ns - note.timestamp > BadWindow && !progress.ln_active) { // LNs are only advanced by being released
			progress.next_note += 1;
			notes_judged += 1;
			apply_judgment({Judgment::Type::Poor}, type);
			if (progress.next_note >= lane.notes.size())
				progress.active_slot = lane.notes[progress.next_note].wav_slot;
		}
	}

	// Advance sample playback
	auto play_slot = [&](vector<dev::Sample> const& slot, WavSlotProgress& progress) {
		if (progress.playback_pos == WavSlotProgress::Stopped) return;
		auto const result = slot[progress.playback_pos];
		progress.playback_pos += 1;
		if (progress.playback_pos >= slot.size())
			progress.playback_pos = WavSlotProgress::Stopped;
		func(result);
		chart_ended = false;
	};
	for (auto [slot, progress]: views::zip(chart->media.wav_slots, wav_slot_progress)) {
		play_slot(slot, progress);
	}

	return chart_ended;
}

template<callable<void(Note const&, Lane::Type, float)> Func>
void Cursor::upcoming_notes(float max_units, Func&& func, nanoseconds offset, bool adjust_for_latency) const
{
	auto const latency_adjustment = adjust_for_latency? -audio::Mixer::get_latency() : 0ns;
	auto const progress_timestamp = dev::Audio::samples_to_ns(sample_progress) + latency_adjustment - offset;
	auto const& bpm_section = get_bpm_section(progress_timestamp, chart->timeline.bpm_sections);
	auto const section_progress = progress_timestamp - bpm_section.position;
	auto const beat_duration = duration<double>{60.0 / chart->metadata.bpm_range.main};
	auto const bpm_ratio = bpm_section.bpm / chart->timeline.bpm_sections[0].bpm;
	auto const current_y = bpm_section.y_pos + section_progress / beat_duration * bpm_ratio * bpm_section.scroll_speed;
	for (auto [idx, lane, progress]: views::zip(views::iota(0zu), chart->timeline.lanes, lane_progress) |
		views::filter([](auto const& tuple) { return get<1>(tuple).visible; })) {
		for (auto const& note: span{lane.notes.begin() + progress.next_note, lane.notes.size() - progress.next_note}) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			func(note, static_cast<Lane::Type>(idx), distance);
		}
	}
}

}
