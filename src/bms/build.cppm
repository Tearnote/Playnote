/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/build.cppm:
Construction of a chart from an IR.
*/

module;
#include "macros/assert.hpp"
#include "macros/logger.hpp"

export module playnote.bms.build;

import playnote.preamble;
import playnote.logger;
import playnote.lib.ebur128;
import playnote.dev.audio;
import playnote.io.bulk_request;
import playnote.io.audio_codec;
import playnote.bms.cursor;
import playnote.bms.chart;
import playnote.bms.ir;

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

// Mappings from slots to external resources. A calue might be empty if the chart didn't define
// a mapping.
struct FileReferences {
	vector<string> wav;
};

void LaneBuilder::add_note(AbsNote const& note)
{
	if (note.type_is<Simple>())
		notes.emplace_back(note);
	else if (note.type_is<LNToggle>())
		ln_ends.emplace_back(note);
	else PANIC();
}

auto LaneBuilder::build(bool deduplicate) -> Lane
{
	auto result = Lane{};

	convert_simple(notes, result.notes);
	convert_ln(ln_ends, result.notes);
	sort_and_deduplicate(result.notes, deduplicate);

	notes.clear();
	ln_ends.clear();
	return result;
}

void LaneBuilder::convert_simple(vector<AbsNote> const& notes, vector<Note>& result)
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

void LaneBuilder::convert_ln(vector<AbsNote>& ln_ends, vector<Note>& result)
{
	stable_sort(ln_ends, [](auto const& a, auto const& b) { return a.position.timestamp < b.position.timestamp; });
	if (ln_ends.size() % 2 != 0) {
		WARN("Unpaired LN end found; chart is most likely invalid");
		ln_ends.pop_back();
	}
	transform(ln_ends | views::chunk(2), back_inserter(result), [&](auto ends) {
		auto const ln_length = ends[1].position.timestamp - ends[0].position.timestamp;
		auto const ln_height = ends[1].position.y_pos - ends[0].position.y_pos;
		return Note{
			.type = Note::LN{
				.length = ln_length,
				.height = static_cast<float>(ln_height),
			},
			.timestamp = ends[0].position.timestamp,
			.y_pos = ends[0].position.y_pos,
			.wav_slot = ends[0].wav_slot,
		};
	});
}

void LaneBuilder::sort_and_deduplicate(vector<Note>& result, bool deduplicate)
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

auto channel_to_note_type(IR::ChannelEvent::Type ch) -> RelativeNoteType
{
	using ChannelType = IR::ChannelEvent::Type;
	if (ch >= ChannelType::BGM && ch <= ChannelType::Note_P2_KeyS) return Simple{};
	if (ch >= ChannelType::Note_P1_Key1_LN && ch <= ChannelType::Note_P2_KeyS_LN) return LNToggle{};
	PANIC();
}

auto channel_to_lane(IR::ChannelEvent::Type ch) -> Chart::LaneType
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

void extend_measure_lengths(vector<double>& measure_lengths, usize max_measure)
{
	auto const min_length = max_measure + 1;
	if (measure_lengths.size() >= min_length) return;
	measure_lengths.resize(min_length, 1.0);
}

void set_measure_length(vector<double>& measure_lengths, usize measure, double length)
{
	extend_measure_lengths(measure_lengths, measure);
	measure_lengths[measure] = length;
}

auto process_ir_headers(Chart& chart, IR const& ir, FileReferences& file_references) -> SlotValues
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

void process_ir_channels(IR const& ir, SlotValues const& slot_values, vector<MeasureRelNote>& notes, vector<MeasureRelBPM>& bpms, vector<double>& measure_lengths)
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

auto build_bpm_relative_measures(span<double const> measure_lengths) -> vector<BeatRelMeasure>
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

auto measure_rel_notes_to_beat_rel(span<MeasureRelNote const> notes, span<BeatRelMeasure const> measures) -> vector<BeatRelNote>
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

void generate_measure_lines(vector<BeatRelNote>& notes, span<BeatRelMeasure const> measures)
{
	transform(measures, back_inserter(notes), [](auto const& measure) {
		return BeatRelNote{
			.type = Simple{},
			.lane = Chart::LaneType::MeasureLine,
			.position = measure.start,
		};
	});
}

auto measure_rel_bpms_to_beat_rel(span<MeasureRelBPM const> bpms, span<BeatRelMeasure const> measures) -> vector<BeatRelBPM>
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

auto beat_rel_notes_to_abs(span<BeatRelNote const> notes, span<BeatRelBPM const> beat_rel_bpms, span<BPMChange const> bpm_changes) -> vector<AbsNote>
{
	auto result = vector<AbsNote>{};
	result.reserve(notes.size());
	transform(notes, back_inserter(result), [&](BeatRelNote const& note) {
		auto bpm_section =
			views::zip(beat_rel_bpms, bpm_changes) |
			views::reverse |
			views::filter([&](auto const& view) {
				if (note.position >= get<0>(view).position) return true;
				return false;
			}) |
			views::take(1);
		ASSERT(!bpm_section.empty());
		auto [beat_rel_bpm, bpm] = *bpm_section.begin();

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

auto build_bpm_changes(span<BeatRelBPM const> bpms) -> vector<BPMChange>
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
	transform(bpms | views::pairwise, back_inserter(result), [&](auto const& bpm_pair) {
		auto [prev_bpm, bpm] = bpm_pair;
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
		return bpm_change;
	});

	return result;
}

void build_lanes(Chart& chart, span<AbsNote const> notes)
{
	auto lane_builders = array<LaneBuilder, +Chart::LaneType::Size>{};

	for (auto const& note: notes)
		lane_builders[+note.lane].add_note(note);

	for (auto [idx, lane]: chart.lanes | views::enumerate) {
		auto const is_bgm = idx == +Chart::LaneType::BGM;
		auto const is_measure_line = idx == +Chart::LaneType::MeasureLine;
		lane = lane_builders[idx].build(!is_bgm);
		if (!is_bgm && !is_measure_line) lane.playable = true;
		if (!is_bgm) lane.visible = true;
		if (!is_measure_line) lane.audible = true;
	}
}

[[nodiscard]] auto measure_loudness(Chart const& chart) -> double
{
	constexpr auto BufferSize = 4096zu / sizeof(dev::Sample);

	auto cursor = Cursor{chart};
	auto ctx = r128::init(dev::Audio::get_sampling_rate());
	auto buffer = vector<dev::Sample>{};
	buffer.reserve(BufferSize);

	auto processing = true;
	while (processing) {
		for (auto _: views::iota(0zu, BufferSize)) {
			auto& sample = buffer.emplace_back();
			processing = !cursor.advance_one_sample([&](auto new_sample) {
				sample.left += new_sample.left;
				sample.right += new_sample.right;
			});
			if (!processing) break;
		}

		r128::add_frames(ctx, buffer);
		buffer.clear();
	}

	auto const result = r128::get_loudness(ctx);

	r128::cleanup(ctx);
	return result;
}

[[nodiscard]] auto lufs_to_gain(double lufs) -> float
{
	constexpr auto LufsTarget = -14.0;
	auto const db_from_target = LufsTarget - lufs;
	auto const amplitude_ratio = pow(10.0, db_from_target / 20.0);
	return static_cast<float>(amplitude_ratio);
}

void calculate_metrics(Chart& chart)
{
	chart.metrics.note_count = fold_left(chart.lanes, 0u, [](auto acc, auto const& lane) {
		return acc + (lane.playable? lane.notes.size() : 0);
	});
	chart.metrics.loudness = measure_loudness(chart);
	chart.metrics.gain = lufs_to_gain(chart.metrics.loudness);
}

// Generate a Chart from an IR. Requires a function to handle the loading of a bulk request.
// The provided function must block until the bulk request is complete.
export template<callable<void(io::BulkRequest&)> Func>
auto chart_from_ir(IR const& ir, Func file_loader) -> shared_ptr<Chart const>
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
	for (auto const& [needed, request, wav_slot]: views::zip(needed_slots, file_references.wav, chart->wav_slots)) {
		if (!needed || request.empty()) continue;
		requests.enqueue<io::AudioCodec>(wav_slot, request, AudioExtensions, false);
	}
	file_loader(requests);

	// Metrics require a fully loaded chart
	calculate_metrics(*chart);

	INFO("Built chart \"{}\"", chart->metadata.title);

	return chart;
}

}
