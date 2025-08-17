/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/build.hpp:
Construction of a chart from an IR.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"
#include "lib/ebur128.hpp"
#include "dev/audio.hpp"
#include "io/bulk_request.hpp"
#include "io/audio_codec.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"
#include "bms/ir.hpp"
#include "threads/audio_shouts.hpp"

namespace playnote::bms {

namespace r128 = lib::ebur128;

// This is out of RelativeNote so that it's not unique for each template argument.
struct Simple {};
struct LNToggle {};
using RelativeNoteType = variant<Simple, LNToggle>;

// A note of a chart with its timing information relative to unknowns such as measure length
// and BPM. LNs are represented as unpaired ends.
template<typename T>
struct RelativeNote {
	using Type = RelativeNoteType;

	Type type;
	Chart::LaneType lane;
	T position;
	usize wav_slot;

	template<variant_alternative<Type> U>
	[[nodiscard]] auto type_is() const -> bool { return holds_alternative<U>(type); }

	template<variant_alternative<Type> U>
	[[nodiscard]] auto params() -> T& { return get<U>(type); }
	template<variant_alternative<Type> U>
	[[nodiscard]] auto params() const -> T const& { return get<U>(type); }
};

// A note where position is represented by a fraction. The whole part is the measure number,
// and the fractional part is position within the measure.
using MeasureRelNote = RelativeNote<NotePosition>;

// A note where position is represented in "units". A unit is equal to one beat, at whatever BPM
// is currently active. A standard measure is 4 units long, but this can change.
using BeatRelNote = RelativeNote<double>;

struct AbsPosition {
	nanoseconds timestamp;
	double y_pos;
};

// A note with known absolute timing and y-position.
using AbsNote = RelativeNote<AbsPosition>;

// The position of a measure within a chart, in BPM-relative units.
struct BeatRelMeasure {
	double start;
	double length;
};

// A BPM change event, measure-relative.
struct MeasureRelBPM {
	NotePosition position;
	float bpm;
	float scroll_speed;
};

// A BPM change event, beat-relative.
struct BeatRelBPM {
	double position;
	float bpm;
	float scroll_speed;
};

// Temporary storage for slot values that don't need to be known during playback.
struct SlotValues {
	vector<float> bpmxx;

	template<typename T>
	static void store(vector<T>& slots, usize slot_id, T value)
	{
		if (slot_id >= slots.size()) slots.resize(slot_id + 1);
		slots[slot_id] = value;
	}

	template<typename T>
	static auto fetch(vector<T> const& slots, usize slot_id) -> T
	{
		if (slot_id >= slots.size()) return T{};
		return slots[slot_id];
	}
};

// Factory that accumulates AbsNotes, then converts them in bulk to a Lane.
class LaneBuilder {
public:
	LaneBuilder() = default;

	// Enqueue an AbsNote. Notes can be enqueued in any order.
	void add_note(AbsNote const& note);

	// Convert enqueued notes to a Lane and clear the queue.
	auto build(bool deduplicate = true) -> Lane;

private:
	vector<AbsNote> notes;
	vector<AbsNote> ln_ends;

	static void convert_simple(vector<AbsNote> const&, vector<Note>&);
	static void convert_ln(vector<AbsNote>&, vector<Note>&);
	static void sort_and_deduplicate(vector<Note>&, bool deduplicate);
};

// Mappings from slots to external resources. A value might be empty if the chart didn't define
// a mapping.
struct FileReferences {
	vector<string> wav;
};

inline void LaneBuilder::add_note(AbsNote const& note)
{
	if (note.type_is<Simple>())
		notes.emplace_back(note);
	else if (note.type_is<LNToggle>())
		ln_ends.emplace_back(note);
	else PANIC();
}

inline auto LaneBuilder::build(bool deduplicate) -> Lane
{
	auto result = Lane{};

	convert_simple(notes, result.notes);
	convert_ln(ln_ends, result.notes);
	sort_and_deduplicate(result.notes, deduplicate);

	notes.clear();
	ln_ends.clear();
	return result;
}

inline void LaneBuilder::convert_simple(vector<AbsNote> const& notes, vector<Note>& result)
{
	transform(notes, back_inserter(result), [&](AbsNote const& note) {
		ASSERT(note.type_is<Simple>());
		return Note{
			.type = Note::Simple{},
			.timestamp = note.position.timestamp,
			.y_pos = note.position.y_pos,
			.wav_slot = note.wav_slot,
		};
	});
}

inline void LaneBuilder::convert_ln(vector<AbsNote>& ln_ends, vector<Note>& result)
{
	stable_sort(ln_ends, [](auto const& a, auto const& b) { return a.position.timestamp < b.position.timestamp; });
	if (ln_ends.size() % 2 != 0) {
		WARN("Unpaired LN end found; chart is most likely invalid");
		ln_ends.pop_back();
	}
	for (auto idx: irange(0zu, ln_ends.size(), 2)) {
		auto const& begin = ln_ends[idx];
		auto const& end = ln_ends[idx + 1];
		auto const ln_length = end.position.timestamp - begin.position.timestamp;
		auto const ln_height = end.position.y_pos - begin.position.y_pos;
		result.emplace_back(Note{
			.type = Note::LN{
				.length = ln_length,
				.height = static_cast<float>(ln_height),
			},
			.timestamp = begin.position.timestamp,
			.y_pos = begin.position.y_pos,
			.wav_slot = begin.wav_slot,
		});
	};
}

inline void LaneBuilder::sort_and_deduplicate(vector<Note>& result, bool deduplicate)
{
	if (!deduplicate) {
		stable_sort(result, [](auto const& a, auto const& b) {
			return a.timestamp < b.timestamp;
		});
		return;
	}
	// std::unique keeps the first of the two duplicate elements while we want to keep the second,
	// so the range is reversed first
	stable_sort(result, [](auto const& a, auto const& b) {
		return a.timestamp > b.timestamp; // Reverse sort
	});
	auto removed = unique(result, [](auto const& a, auto const& b) {
		return a.timestamp == b.timestamp;
	});
	auto removed_count = removed.size();
	result.erase(removed.begin(), removed.end());
	if (removed_count) INFO("Removed {} duplicate notes", removed_count);
	reverse(result); // Reverse back
}

inline auto channel_to_note_type(IR::ChannelEvent::Type ch) -> RelativeNoteType
{
	using ChannelType = IR::ChannelEvent::Type;
	if (ch >= ChannelType::BGM && ch <= ChannelType::Note_P2_KeyS) return Simple{};
	if (ch >= ChannelType::Note_P1_Key1_LN && ch <= ChannelType::Note_P2_KeyS_LN) return LNToggle{};
	PANIC();
}

inline auto channel_to_lane(IR::ChannelEvent::Type ch) -> Chart::LaneType
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM:
		return Chart::LaneType::BGM;
	case IR::ChannelEvent::Type::Note_P1_Key1:
	case IR::ChannelEvent::Type::Note_P1_Key1_LN:
		return Chart::LaneType::P1_Key1;
	case IR::ChannelEvent::Type::Note_P1_Key2:
	case IR::ChannelEvent::Type::Note_P1_Key2_LN:
		return Chart::LaneType::P1_Key2;
	case IR::ChannelEvent::Type::Note_P1_Key3:
	case IR::ChannelEvent::Type::Note_P1_Key3_LN:
		return Chart::LaneType::P1_Key3;
	case IR::ChannelEvent::Type::Note_P1_Key4:
	case IR::ChannelEvent::Type::Note_P1_Key4_LN:
		return Chart::LaneType::P1_Key4;
	case IR::ChannelEvent::Type::Note_P1_Key5:
	case IR::ChannelEvent::Type::Note_P1_Key5_LN:
		return Chart::LaneType::P1_Key5;
	case IR::ChannelEvent::Type::Note_P1_Key6:
	case IR::ChannelEvent::Type::Note_P1_Key6_LN:
		return Chart::LaneType::P1_Key6;
	case IR::ChannelEvent::Type::Note_P1_Key7:
	case IR::ChannelEvent::Type::Note_P1_Key7_LN:
		return Chart::LaneType::P1_Key7;
	case IR::ChannelEvent::Type::Note_P1_KeyS:
	case IR::ChannelEvent::Type::Note_P1_KeyS_LN:
		return Chart::LaneType::P1_KeyS;
	case IR::ChannelEvent::Type::Note_P2_Key1:
	case IR::ChannelEvent::Type::Note_P2_Key1_LN:
		return Chart::LaneType::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2:
	case IR::ChannelEvent::Type::Note_P2_Key2_LN:
		return Chart::LaneType::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3:
	case IR::ChannelEvent::Type::Note_P2_Key3_LN:
		return Chart::LaneType::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4:
	case IR::ChannelEvent::Type::Note_P2_Key4_LN:
		return Chart::LaneType::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5:
	case IR::ChannelEvent::Type::Note_P2_Key5_LN:
		return Chart::LaneType::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6:
	case IR::ChannelEvent::Type::Note_P2_Key6_LN:
		return Chart::LaneType::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7:
	case IR::ChannelEvent::Type::Note_P2_Key7_LN:
		return Chart::LaneType::P2_Key7;
	case IR::ChannelEvent::Type::Note_P2_KeyS:
	case IR::ChannelEvent::Type::Note_P2_KeyS_LN:
		return Chart::LaneType::P2_KeyS;
	default: return Chart::LaneType::Size;
	}
}

inline void extend_measure_lengths(vector<double>& measure_lengths, usize max_measure)
{
	auto const min_length = max_measure + 1;
	if (measure_lengths.size() >= min_length) return;
	measure_lengths.resize(min_length, 1.0);
}

inline void set_measure_length(vector<double>& measure_lengths, usize measure, double length)
{
	extend_measure_lengths(measure_lengths, measure);
	measure_lengths[measure] = length;
}

inline auto process_ir_headers(Chart& chart, IR const& ir, FileReferences& file_references) -> SlotValues
{
	auto slot_values = SlotValues{};
	ir.each_header_event([&](IR::HeaderEvent const& event) {
		visit(visitor {
			[&](IR::HeaderEvent::Title* params) { chart.metadata.title = params->title; },
			[&](IR::HeaderEvent::Subtitle* params) { chart.metadata.subtitle = params->subtitle; },
			[&](IR::HeaderEvent::Artist* params) { chart.metadata.artist = params->artist; },
			[&](IR::HeaderEvent::Subartist* params) { chart.metadata.subartist = params->subartist; },
			[&](IR::HeaderEvent::Genre* params) { chart.metadata.genre = params->genre; },
			[&](IR::HeaderEvent::URL* params) { chart.metadata.url = params->url; },
			[&](IR::HeaderEvent::Email* params) { chart.metadata.email = params->email; },
			[&](IR::HeaderEvent::Difficulty* params) { chart.metadata.difficulty = params->level; },
			[&](IR::HeaderEvent::BPM* params) { chart.bpm = params->bpm; },
			[&](IR::HeaderEvent::WAV* params) { file_references.wav[params->slot] = params->name; },
			[&](IR::HeaderEvent::BPMxx* params) { slot_values.store(slot_values.bpmxx, params->slot, params->bpm); },
			[](auto&&) {}
		}, event.params);
	});
	return slot_values;
}

inline void process_ir_channels(IR const& ir, SlotValues const& slot_values, vector<MeasureRelNote>& notes, vector<MeasureRelBPM>& bpms, vector<double>& measure_lengths)
{
	ir.each_channel_event([&](IR::ChannelEvent const& event) {
		if (event.type == IR::ChannelEvent::Type::MeasureLength) {
			set_measure_length(measure_lengths, trunc(event.position), bit_cast<double>(event.slot));
			return;
		}
		if (event.type == IR::ChannelEvent::Type::BPM) {
			auto const bpm = static_cast<float>(event.slot);
			if (bpm <= 0.0) return;
			bpms.emplace_back(MeasureRelBPM{
				.position = event.position,
				.bpm = bpm,
				.scroll_speed = 1.0f,
			});
			return;
		}
		if (event.type == IR::ChannelEvent::Type::BPMxx) {
			auto const bpm = slot_values.fetch(slot_values.bpmxx, event.slot);
			if (bpm <= 0.0) return;
			bpms.emplace_back(MeasureRelBPM{
				.position = event.position,
				.bpm = bpm,
				.scroll_speed = 1.0f,
			});
			return;
		}
		auto const lane_id = channel_to_lane(event.type);
		if (lane_id == Chart::LaneType::Size) return;
		notes.emplace_back(MeasureRelNote{
			.type = channel_to_note_type(event.type),
			.lane = lane_id,
			.position = event.position,
			.wav_slot = event.slot,
		});
		extend_measure_lengths(measure_lengths, trunc(event.position));
	});
}

inline auto build_bpm_relative_measures(span<double const> measure_lengths) -> vector<BeatRelMeasure>
{
	auto result = vector<BeatRelMeasure>{};
	result.reserve(measure_lengths.size());

	auto cursor = 0.0;
	transform(measure_lengths, back_inserter(result), [&](auto length) {
		auto const measure = BeatRelMeasure{
			.start = cursor,
			.length = length * 4.0,
		};
		cursor += measure.length;
		return measure;
	});
	return result;
}

inline auto measure_rel_notes_to_beat_rel(span<MeasureRelNote const> notes, span<BeatRelMeasure const> measures) -> vector<BeatRelNote>
{
	auto result = vector<BeatRelNote>{};
	result.reserve(notes.size());
	transform(notes, back_inserter(result), [&](MeasureRelNote const& note) {
		auto const& measure = measures[trunc(note.position)];
		auto const position = measure.start + measure.length * rational_cast<double>(fract(note.position));
		return BeatRelNote{
			.type = note.type,
			.lane = note.lane,
			.position = position,
			.wav_slot = note.wav_slot,
		};
	});
	return result;
}

inline void generate_measure_lines(vector<BeatRelNote>& notes, span<BeatRelMeasure const> measures)
{
	transform(measures, back_inserter(notes), [](auto const& measure) {
		return BeatRelNote{
			.type = Simple{},
			.lane = Chart::LaneType::MeasureLine,
			.position = measure.start,
		};
	});
}

inline auto measure_rel_bpms_to_beat_rel(span<MeasureRelBPM const> bpms, span<BeatRelMeasure const> measures) -> vector<BeatRelBPM>
{
	auto result = vector<BeatRelBPM>{};
	result.reserve(bpms.size());
	transform(bpms, back_inserter(result), [&](MeasureRelBPM const& bpm) {
		auto const& measure = measures[trunc(bpm.position)];
		auto const position = measure.start + measure.length * rational_cast<double>(fract(bpm.position));
		return BeatRelBPM{
			.position = position,
			.bpm = bpm.bpm,
			.scroll_speed = bpm.scroll_speed,
		};
	});
	return result;
}

inline auto beat_rel_notes_to_abs(span<BeatRelNote const> notes, span<BeatRelBPM const> beat_rel_bpms, span<BPMChange const> bpm_changes) -> vector<AbsNote>
{
	auto result = vector<AbsNote>{};
	result.reserve(notes.size());
	transform(notes, back_inserter(result), [&](BeatRelNote const& note) {
		auto bpm_section = find_last_if(beat_rel_bpms, [&](auto const& bpm) {
			return note.position >= bpm.position;
		});
		ASSERT(!bpm_section.empty());
		auto const& beat_rel_bpm = *bpm_section.begin();
		auto const& bpm = bpm_changes[distance(beat_rel_bpms.begin(), bpm_section.begin())];

		auto const beats_since_bpm = note.position - beat_rel_bpm.position;
		auto const time_since_bpm = beats_since_bpm * duration<double>{60.0 / bpm.bpm};
		auto const timestamp = bpm.position + duration_cast<nanoseconds>(time_since_bpm);
		auto const y_pos = bpm.y_pos + beats_since_bpm * bpm.scroll_speed;
		return AbsNote{
			.type = note.type,
			.lane = note.lane,
			.position = AbsPosition{
				.timestamp = timestamp,
				.y_pos = y_pos,
			},
			.wav_slot = note.wav_slot,
		};
	});
	return result;
}

inline auto build_bpm_changes(span<BeatRelBPM const> bpms) -> vector<BPMChange>
{
	auto result = vector<BPMChange>{};
	result.reserve(bpms.size());

	// To establish the timestamp of a BPM change, we need to know the BPM of the previous one,
	// and the number of beats since then. So, the first one needs to be handled manually.
	ASSERT(!bpms.empty());
	result.emplace_back(BPMChange{
		.position = 0ns,
		.bpm = bpms[0].bpm,
		.y_pos = 0.0,
		.scroll_speed = 1.0f,
	});

	auto cursor = 0ns;
	auto y_cursor = 0.0;
	for (auto idx: irange(1zu, bpms.size())) {
		auto const& bpm = bpms[idx];
		auto const& prev_bpm = bpms[idx - 1];
		auto const beats_elapsed = bpm.position - prev_bpm.position;
		auto const time_elapsed = duration_cast<nanoseconds>(beats_elapsed * duration<double>{60.0 / prev_bpm.bpm});
		cursor += time_elapsed;
		y_cursor += beats_elapsed * prev_bpm.scroll_speed;
		auto const bpm_change = BPMChange{
			.position = cursor,
			.bpm = bpm.bpm,
			.y_pos = y_cursor,
			.scroll_speed = bpm.scroll_speed,
		};
		result.emplace_back(bpm_change);
	}

	return result;
}

inline void build_lanes(Chart& chart, span<AbsNote const> notes)
{
	auto lane_builders = array<LaneBuilder, +Chart::LaneType::Size>{};

	for (auto const& note: notes)
		lane_builders[+note.lane].add_note(note);

	for (auto idx: irange(0zu, chart.lanes.size())) {
		auto& lane = chart.lanes[idx];
		auto const is_bgm = idx == +Chart::LaneType::BGM;
		auto const is_measure_line = idx == +Chart::LaneType::MeasureLine;
		lane = lane_builders[idx].build(!is_bgm);
		if (!is_bgm && !is_measure_line) lane.playable = true;
		if (!is_bgm) lane.visible = true;
		if (!is_measure_line) lane.audible = true;
	}
}

[[nodiscard]] inline auto lufs_to_gain(double lufs) -> float
{
	constexpr auto LufsTarget = -14.0;
	auto const db_from_target = LufsTarget - lufs;
	auto const amplitude_ratio = pow(10.0, db_from_target / 20.0);
	return static_cast<float>(amplitude_ratio);
}

inline auto determine_playstyle(Chart::Lanes const& lanes) -> Playstyle
{
	using enum Chart::LaneType;
	auto lanes_used = array<bool, lanes.size()>{};
	transform(lanes, lanes_used.begin(), [](auto const& lane) { return !lane.notes.empty(); });

	if (lanes_used[+P2_Key6] ||
		lanes_used[+P2_Key7])
		return Playstyle::_14K;
	if (lanes_used[+P2_Key1] ||
		lanes_used[+P2_Key2] ||
		lanes_used[+P2_Key3] ||
		lanes_used[+P2_Key4] ||
		lanes_used[+P2_Key5] ||
		lanes_used[+P2_KeyS])
		return Playstyle::_10K;
	if (lanes_used[+P1_Key6] ||
		lanes_used[+P1_Key7])
		return Playstyle::_7K;
	if (lanes_used[+P1_Key1] ||
		lanes_used[+P1_Key2] ||
		lanes_used[+P1_Key3] ||
		lanes_used[+P1_Key4] ||
		lanes_used[+P1_Key5] ||
		lanes_used[+P1_KeyS])
		return Playstyle::_5K;
	return Playstyle::_7K; // Empty chart, but sure whatever
}

inline void calculate_note_metrics(Chart::Lanes const& lanes, Metrics& metrics)
{
	metrics.note_count = fold_left(lanes, 0u, [](auto acc, auto const& lane) {
		return acc + (lane.playable? lane.notes.size() : 0);
	});
	metrics.chart_duration = fold_left(lanes,
		0ns, [](auto acc, Lane const& lane) {
			if (lane.notes.empty() || !lane.playable) return acc;
			auto const& last_note = lane.notes.back();
			auto note_end = last_note.timestamp;
			if (last_note.type_is<Note::LN>()) note_end += last_note.params<Note::LN>().length;
			return max(acc, note_end);
		}
	);
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
void calculate_audio_metrics(Cursor&& cursor, Metrics& metrics, Func&& progress)
{
	constexpr auto BufferSize = 4096zu / sizeof(dev::Sample);

	auto ctx = r128::init(dev::Audio::get_sampling_rate());
	auto buffer = vector<dev::Sample>{};
	buffer.reserve(BufferSize);

	auto processing = true;
	while (processing) {
		for (auto _: irange(0zu, BufferSize)) {
			auto& sample = buffer.emplace_back();
			processing = !cursor.advance_one_sample([&](auto new_sample) {
				sample.left += new_sample.left;
				sample.right += new_sample.right;
			}, false);
			if (!processing) break;
		}

		progress(threads::ChartLoadProgress::Measuring{ .progress = cursor.get_progress_ns() });
		r128::add_frames(ctx, buffer);
		buffer.clear();
	}

	metrics.loudness = r128::get_loudness(ctx);
	metrics.gain = lufs_to_gain(metrics.loudness);
	metrics.audio_duration = cursor.get_progress_ns();
	r128::cleanup(ctx);
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
auto calculate_density_distribution(Chart::Lanes const& lanes, nanoseconds chart_duration,
	nanoseconds resolution, nanoseconds window, Func&& progress) -> Density
{
	constexpr auto Bandwidth = 3.0f; // in standard deviations
	// scale back a stretched window, and correct for considering only 3 standard deviations
	auto const GaussianScale = 1.0f / (window / 1s) * (1.0f / 0.973f);

	auto result = Density{};
	auto const points = chart_duration / resolution + 1;
	result.key_density.resize(points);
	result.scratch_density.resize(points);
	result.ln_density.resize(points);

	auto const ProgressFreq = static_cast<uint32>(floor(1s / window));
	auto until_progress_update = 0u;
	for (auto idx: irange(0zu, result.key_density.size())) {
		auto const cursor = static_cast<isize>(idx) * resolution;
		auto& key = result.key_density[idx];
		auto& scratch = result.scratch_density[idx];
		auto& ln = result.ln_density[idx];
		for (auto l_idx: irange(0zu, lanes.size())) {
			auto const& lane = lanes[l_idx];
			auto const type = Chart::LaneType{l_idx};
			if (!lane.playable) continue;
			for (Note const& note: lane.notes) {
				if (note.timestamp < cursor - window) continue;
				if (note.timestamp > cursor + window) break;
				auto& target = [&]() -> float& {
					if (type == Chart::LaneType::P1_KeyS || type == Chart::LaneType::P2_KeyS) return scratch;
					if (note.type_is<Note::LN>()) return ln;
					return key;
				}();
				auto const delta = note.timestamp - cursor;
				auto const delta_scaled = ratio(delta, window) * Bandwidth; // now within [-Bandwidth, Bandwidth]
				target += exp(-pow(delta_scaled, 2.0f) / 2.0f) * GaussianScale; // Gaussian filter
			}
		}

		until_progress_update += 1;
		if (until_progress_update >= ProgressFreq) {
			until_progress_update = 0;
			progress(threads::ChartLoadProgress::DensityCalculation{ .progress = cursor });
		}
	}
	return result;
}

inline auto calculate_features(Chart const& chart) -> Features
{
	auto result = Features{};
	result.has_ln = any_of(chart.lanes, [](auto const& lane) {
		if (!lane.playable) return false;
		return any_of(lane.notes, [](Note const& note) {
			return note.type_is<Note::LN>();
		});
	});
	result.has_soflan = chart.bpm_changes.size() > 1;
	return result;
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
void calculate_metrics(Chart& chart, Func&& progress)
{
	chart.metrics.playstyle = determine_playstyle(chart.lanes);
	calculate_note_metrics(chart.lanes, chart.metrics);
	calculate_audio_metrics(Cursor{chart}, chart.metrics, progress);
	chart.metrics.density = calculate_density_distribution(chart.lanes, chart.metrics.chart_duration, 125ms, 2s, progress);
	chart.metrics.features = calculate_features(chart);
}

inline void calculate_bb(Chart& chart)
{
	chart.slot_bb.windows.resize(chart.metrics.audio_duration / SlotBB::WindowSize + 1);

	for (auto const& lane: chart.lanes) {
		if (!lane.audible) continue;
		for (auto idx: irange(0zu, lane.notes.size())) {
			auto const& note = lane.notes[idx];
			if (chart.wav_slots[note.wav_slot].empty()) continue;
			// Register note audio in the structure
			auto const wav_len = dev::Audio::samples_to_ns(chart.wav_slots[note.wav_slot].size());
			auto const next_note_start = idx >= lane.notes.size() - 1? chart.metrics.audio_duration :
				lane.playable? lane.notes[idx + 1].timestamp : note.timestamp;
			auto const start = note.timestamp;
			auto const end = next_note_start + wav_len;
			auto const first_window = clamp<usize>(start / SlotBB::WindowSize, 0zu, chart.slot_bb.windows.size() - 1);
			auto const last_window = clamp<usize>(end / SlotBB::WindowSize + 1, 0zu, chart.slot_bb.windows.size() - 1);
			for (auto w_idx: irange(first_window, last_window + 1)) {
				auto& window = chart.slot_bb.windows[w_idx];
				if (contains(window, note.wav_slot)) continue;
				if (window.size() == window.capacity()) {
					WARN("Unable to add sample slot to bounding box; reached limit of {}", SlotBB::MaxSlots);
					continue;
				}
				window.push_back(note.wav_slot);
			}
		}
	}

	auto biggest_window = 0zu;
	for (auto const& window: chart.slot_bb.windows) {
		biggest_window = max(biggest_window, window.size());
	}
}

// Generate a Chart from an IR. Requires a function to handle the loading of a bulk request.
// The provided function must block until the bulk request is complete.
template<callable<void(io::BulkRequest&)> Func, callable<void(threads::ChartLoadProgress::Type)> Func2>
auto chart_from_ir(IR const& ir, Func&& file_loader, Func2&& progress) -> shared_ptr<Chart const>
{
	static constexpr auto AudioExtensions = {"wav"sv, "ogg"sv, "mp3"sv, "flac"sv, "opus"sv};

	auto chart = make_shared<Chart>();
	auto measure_rel_notes = vector<MeasureRelNote>{};
	auto measure_rel_bpms = vector<MeasureRelBPM>{};
	auto measure_lengths = vector<double>{};
	auto file_references = FileReferences{};

	auto const domain = ir.get_path().parent_path();
	measure_lengths.reserve(256); // Arbitrary; enough for most charts
	chart->wav_slots.resize(ir.get_wav_slot_count());
	file_references.wav.clear();
	file_references.wav.resize(ir.get_wav_slot_count());

	auto const slot_values = process_ir_headers(*chart, ir, file_references);
	measure_rel_bpms.emplace_back(NotePosition{0}, chart->bpm, 1.0f); // Add initial BPM as the first BPM change
	process_ir_channels(ir, slot_values, measure_rel_notes, measure_rel_bpms, measure_lengths);
	stable_sort(measure_rel_bpms, [](auto const& a, auto const& b) { return a.position < b.position; });

	auto const measures = build_bpm_relative_measures(measure_lengths);
	auto beat_rel_notes = measure_rel_notes_to_beat_rel(measure_rel_notes, measures);
	generate_measure_lines(beat_rel_notes, measures);
	auto const beat_rel_bpms = measure_rel_bpms_to_beat_rel(measure_rel_bpms, measures);
	chart->bpm_changes = build_bpm_changes(beat_rel_bpms);
	auto const abs_notes = beat_rel_notes_to_abs(beat_rel_notes, beat_rel_bpms, chart->bpm_changes);
	build_lanes(*chart, abs_notes);

	auto needed_slots = vector<bool>{};
	needed_slots.resize(chart->wav_slots.size(), false);

	// Mark used slots
	for (auto const& lane: chart->lanes) {
		for (auto const& note: lane.notes) {
			needed_slots[note.wav_slot] = true;
		}
	}

	// Enqueue and execute file requests for used slots
	auto requests = io::BulkRequest{domain};
	for (auto idx: irange(0zu, needed_slots.size())) {
		auto const request = string_view{file_references.wav[idx]};
		if (!needed_slots[idx] || request.empty()) continue;
		requests.enqueue<io::AudioCodec>(chart->wav_slots[idx], request, AudioExtensions, false);
	}
	file_loader(requests);

	// Metrics require a fully loaded chart
	calculate_metrics(*chart, progress);
	calculate_bb(*chart);

	INFO("Built chart \"{}\"", chart->metadata.title);

	return chart;
}

}
