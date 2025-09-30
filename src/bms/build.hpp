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
#include "lib/ffmpeg.hpp"
#include "dev/audio.hpp"
#include "io/song_legacy.hpp"
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
	Lane::Type lane;
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

inline auto channel_to_lane(IR::ChannelEvent::Type ch) -> optional<Lane::Type>
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM:
		return Lane::Type::BGM;
	case IR::ChannelEvent::Type::Note_P1_Key1:
	case IR::ChannelEvent::Type::Note_P1_Key1_LN:
		return Lane::Type::P1_Key1;
	case IR::ChannelEvent::Type::Note_P1_Key2:
	case IR::ChannelEvent::Type::Note_P1_Key2_LN:
		return Lane::Type::P1_Key2;
	case IR::ChannelEvent::Type::Note_P1_Key3:
	case IR::ChannelEvent::Type::Note_P1_Key3_LN:
		return Lane::Type::P1_Key3;
	case IR::ChannelEvent::Type::Note_P1_Key4:
	case IR::ChannelEvent::Type::Note_P1_Key4_LN:
		return Lane::Type::P1_Key4;
	case IR::ChannelEvent::Type::Note_P1_Key5:
	case IR::ChannelEvent::Type::Note_P1_Key5_LN:
		return Lane::Type::P1_Key5;
	case IR::ChannelEvent::Type::Note_P1_Key6:
	case IR::ChannelEvent::Type::Note_P1_Key6_LN:
		return Lane::Type::P1_Key6;
	case IR::ChannelEvent::Type::Note_P1_Key7:
	case IR::ChannelEvent::Type::Note_P1_Key7_LN:
		return Lane::Type::P1_Key7;
	case IR::ChannelEvent::Type::Note_P1_KeyS:
	case IR::ChannelEvent::Type::Note_P1_KeyS_LN:
		return Lane::Type::P1_KeyS;
	case IR::ChannelEvent::Type::Note_P2_Key1:
	case IR::ChannelEvent::Type::Note_P2_Key1_LN:
		return Lane::Type::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2:
	case IR::ChannelEvent::Type::Note_P2_Key2_LN:
		return Lane::Type::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3:
	case IR::ChannelEvent::Type::Note_P2_Key3_LN:
		return Lane::Type::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4:
	case IR::ChannelEvent::Type::Note_P2_Key4_LN:
		return Lane::Type::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5:
	case IR::ChannelEvent::Type::Note_P2_Key5_LN:
		return Lane::Type::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6:
	case IR::ChannelEvent::Type::Note_P2_Key6_LN:
		return Lane::Type::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7:
	case IR::ChannelEvent::Type::Note_P2_Key7_LN:
		return Lane::Type::P2_Key7;
	case IR::ChannelEvent::Type::Note_P2_KeyS:
	case IR::ChannelEvent::Type::Note_P2_KeyS_LN:
		return Lane::Type::P2_KeyS;
	default: return nullopt;
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
			[&](IR::HeaderEvent::BPM* params) { chart.metadata.bpm_range.initial = params->bpm; },
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
		if (!lane_id) return;
		notes.emplace_back(MeasureRelNote{
			.type = channel_to_note_type(event.type),
			.lane = *lane_id,
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
			.lane = Lane::Type::MeasureLine,
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
		auto bpm_section = find_last_if(views::zip(beat_rel_bpms, bpm_changes), [&](auto const& view) {
			return note.position >= get<0>(view).position;
		});
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

inline void build_lanes(Chart& chart, span<AbsNote const> notes)
{
	auto lane_builders = array<LaneBuilder, enum_count<Lane::Type>()>{};

	for (auto const& note: notes)
		lane_builders[+note.lane].add_note(note);

	for (auto [idx, lane]: chart.timeline.lanes | views::enumerate) {
		auto const is_bgm = idx == +Lane::Type::BGM;
		auto const is_measure_line = idx == +Lane::Type::MeasureLine;
		lane = lane_builders[idx].build(!is_bgm);
		if (!is_bgm && !is_measure_line) lane.playable = true;
		if (!is_bgm) lane.visible = true;
		if (!is_measure_line) lane.audible = true;
	}
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
void load_files(Chart& chart, io::SongLegacy& song, FileReferences const& references, Func&& progress)
{
	static constexpr auto AudioExtensions = {"wav"sv, "ogg"sv, "mp3"sv, "flac"sv, "opus"sv};

	// Mark used slots
	auto needed_slots = vector<bool>{};
	needed_slots.resize(chart.media.wav_slots.size(), false);
	for (auto const& lane: chart.timeline.lanes) {
		for (auto const& note: lane.notes) {
			needed_slots[note.wav_slot] = true;
		}
	}

	// Enqueue file requests for used slots
	struct Request {
		string_view filename;
		Media::WavSlot& slot;
	};
	auto requests = vector<Request>{};
	requests.reserve(references.wav.size());
	for (auto [needed, request, slot]: views::zip(needed_slots, references.wav, chart.media.wav_slots) |
		views::filter([](auto const& view) { return get<0>(view) && !get<1>(view).empty(); })) {
		requests.emplace_back(Request{
			.filename = request,
			.slot = slot,
		});
	}

	// Load all required samples from disk
	struct Job {
		string_view filename;
		vector<byte> file;
		Media::WavSlot& slot;
	};
	auto jobs = vector<Job>{};
	jobs.reserve(requests.size());
	song.for_each_file([&](auto file_ref) {
		auto filename = fs::path{file_ref.filename};

		// Handle extension matching
		auto ext = filename.extension().string();
		if (find_if(AudioExtensions, [&](auto const& e) { return iequals(e, ext); }))
			filename = filename.replace_extension();
		auto filename_str = filename.string();
		while (filename_str.ends_with("."))
			filename_str.pop_back();

		// Check if we need the file
		auto match = find_if(requests, [&](auto const& req) { return iequals(req.filename, filename_str); });
		if (match == requests.end()) return;

		jobs.emplace_back(Job{
			.filename = match->filename,
			.file = move(file_ref.load()),
			.slot = match->slot,
		});
	});

	// Decode and resample sample files in a thread pool
	auto completions = channel<usize>{};
	auto job_idx = atomic<usize>{0};
	auto worker = [&] {
		while (true) {
			auto const current_job_idx = job_idx.fetch_add(1);
			if (current_job_idx >= jobs.size()) break;

			auto const& job = jobs[current_job_idx];
			try {
				job.slot = move(lib::ffmpeg::decode_and_resample_file_buffer(job.file, dev::Audio::get_sampling_rate()));
				completions << current_job_idx;
			} catch (exception const& e) {
				WARN("Failed to load \"{}\": {}", job.filename, e.what());
				completions << -1zu;
			}
		}
	};
	auto const WorkerCount = jthread::hardware_concurrency();
	auto workers = vector<jthread>{};
	workers.reserve(WorkerCount);
	for (auto _: views::iota(0u, WorkerCount))
		workers.emplace_back(worker);

	auto completion_count = 0zu;
	auto const JobCount = jobs.size();
	auto completed = -1zu;
	while (completions.read(completed)) {
		completion_count += 1;
		progress(threads::ChartLoadProgress::LoadingFiles{
			.loaded = completion_count,
			.total = JobCount,
		});
		if (completion_count >= JobCount) break;
	}
}

inline auto determine_playstyle(Timeline::Lanes const& lanes) -> Playstyle
{
	using enum Lane::Type;
	auto lanes_used = array<bool, enum_count<Lane::Type>()>{};
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

inline void calculate_note_metrics(Timeline::Lanes const& lanes, Metadata& metadata)
{
	metadata.note_count = fold_left(lanes, 0u, [](auto acc, auto const& lane) {
		return acc + (lane.playable? lane.notes.size() : 0);
	});
	metadata.chart_duration = fold_left(lanes |
		views::filter([](auto const& lane) { return !lane.notes.empty() && lane.playable; }) |
		views::transform([](auto const& lane) -> Note const& { return lane.notes.back(); }),
		0ns, [](auto acc, Note const& last_note) {
			auto note_end = last_note.timestamp;
			if (last_note.type_is<Note::LN>()) note_end += last_note.params<Note::LN>().length;
			return max(acc, note_end);
		}
	);
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
void calculate_audio_metrics(Cursor&& cursor, Metadata& metadata, Func&& progress)
{
	constexpr auto BufferSize = 4096zu / sizeof(dev::Sample);

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
			}, {}, false);
			if (!processing) break;
		}

		progress(threads::ChartLoadProgress::Measuring{ .progress = cursor.get_progress_ns() });
		r128::add_frames(ctx, buffer);
		buffer.clear();
	}

	metadata.loudness = r128::get_loudness(ctx);
	metadata.audio_duration = cursor.get_progress_ns();
}

inline auto notes_around(span<Note const> notes, nanoseconds cursor, nanoseconds window)
{
	auto const from = cursor - window;
	auto const to = cursor + window;
	return notes | views::drop_while([=](auto const& note) {
		return note.timestamp < from;
	}) | views::take_while([=](auto const& note) {
		return note.timestamp <= to;
	});
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
void calculate_density_distribution(Metadata& metadata, Timeline::Lanes const& lanes,
	nanoseconds chart_duration, nanoseconds resolution, nanoseconds window, Func&& progress)
{
	constexpr auto Bandwidth = 3.0f; // in standard deviations
	// scale back a stretched window, and correct for considering only 3 standard deviations
	auto const GaussianScale = 1.0f / (window / 1s) * (1.0f / 0.973f);

	auto const points = chart_duration / resolution + 1;
	metadata.density.resolution = resolution;
	metadata.density.key.resize(points);
	metadata.density.scratch.resize(points);
	metadata.density.ln.resize(points);

	// Collect all playable notes
	auto notes_keys = vector<Note>{};
	auto notes_scr = vector<Note>{};
	auto note_total = fold_left(lanes, 0u, [](auto sum, auto const& lane) { return sum + lane.playable? lane.notes.size() : 0; });
	notes_keys.reserve(note_total);
	notes_scr.reserve(note_total);
	for (auto [type, lane]: views::zip(
		views::iota(0u) | views::transform([](auto idx) { return Lane::Type{idx}; }),
		lanes) |
		views::filter([](auto const& view) { return get<1>(view).playable; })) {
		auto& dest = type == Lane::Type::P1_KeyS || type == Lane::Type::P2_KeyS? notes_scr : notes_keys;
		for (Note const& note: lane.notes) {
			if (note.type_is<Note::LN>()) {
				dest.emplace_back(note);
				auto ln_end = note;
				ln_end.timestamp += ln_end.params<Note::LN>().length;
				dest.emplace_back(ln_end);
			} else {
				dest.emplace_back(note);
			}
		}
	}
	sort(notes_keys, [](auto const& left, auto const& right) { return left.timestamp < right.timestamp; });
	sort(notes_scr, [](auto const& left, auto const& right) { return left.timestamp < right.timestamp; });

	auto const ProgressFreq = static_cast<uint32>(floor(1s / window));
	auto until_progress_update = 0u;
	for (auto [cursor, key, scratch, ln]: views::zip(
		views::iota(0u) | views::transform([&](auto i) { return i * resolution; }),
		metadata.density.key, metadata.density.scratch, metadata.density.ln)) {
		for (Note const& note: notes_around(notes_keys, cursor, window)) {
			auto& target = [&]() -> float& {
				if (note.type_is<Note::LN>()) return ln;
				return key;
			}();
			auto const delta = note.timestamp - cursor;
			auto const delta_scaled = ratio(delta, window) * Bandwidth; // now within [-Bandwidth, Bandwidth]
			target += exp(-pow(delta_scaled, 2.0f) / 2.0f) * GaussianScale; // Gaussian filter
		}
		for (Note const& note: notes_around(notes_scr, cursor, window)) {
			auto const delta = note.timestamp - cursor;
			auto const delta_scaled = ratio(delta, window) * Bandwidth; // now within [-Bandwidth, Bandwidth]
			scratch += exp(-pow(delta_scaled, 2.0f) / 2.0f) * GaussianScale; // Gaussian filter
		}

		until_progress_update += 1;
		if (until_progress_update >= ProgressFreq) {
			until_progress_update = 0;
			progress(threads::ChartLoadProgress::DensityCalculation{ .progress = cursor });
		}
	}

	// Average NPS: actually Root Mean Square of the middle 50% of the dataset
	auto overall_density = vector<float>{};
	overall_density.reserve(metadata.density.key.size());
	transform(views::zip(metadata.density.key, metadata.density.scratch, metadata.density.ln), back_inserter(overall_density),
		[](auto const& view) { return get<0>(view) + get<1>(view) + get<2>(view); });
	sort(overall_density);
	auto quarter_size = overall_density.size() / 4;
	auto density_mid50 = span{overall_density.begin() + quarter_size, overall_density.end() - quarter_size};
	auto rms = fold_left(density_mid50, 0.0,
		[&](auto acc, auto val) { return acc + val * val * val * val / density_mid50.size(); });
	metadata.nps.average = sqrt(sqrt(rms));

	// Peak NPS: RMS of the 98th percentile
	auto fiftieth_size = overall_density.size() / 50;
	auto density_top2 = span{overall_density.end() - fiftieth_size, overall_density.end()};
	auto peak_rms = fold_left(density_top2, 0.0,
		[&](auto acc, auto val) { return acc + val * val * val * val / density_top2.size(); });
	metadata.nps.peak = sqrt(sqrt(peak_rms));
}

inline auto calculate_features(Chart const& chart) -> Metadata::Features
{
	auto result = Metadata::Features{};
	result.has_ln = any_of(chart.timeline.lanes, [](auto const& lane) {
		if (!lane.playable) return false;
		return any_of(lane.notes, [](Note const& note) {
			return note.type_is<Note::LN>();
		});
	});
	result.has_soflan = chart.timeline.bpm_sections.size() > 1;
	return result;
}

inline auto calculate_bpm_range(Chart const& chart) -> Metadata::BPMRange
{
	auto bpm_distribution = unordered_map<float, nanoseconds>{};
	auto update = [&](float bpm, nanoseconds duration) {
		bpm_distribution.try_emplace(bpm, 0ns);
		bpm_distribution[bpm] += duration;
	};

	for (auto [current, next]: chart.timeline.bpm_sections | views::pairwise)
		update(current.bpm, next.position - current.position);
	// Last one has no pairing; duration is until the end of the chart
	update(chart.timeline.bpm_sections.back().bpm, chart.metadata.chart_duration - chart.timeline.bpm_sections.back().position);

	auto result = Metadata::BPMRange{
		.min = *min_element(bpm_distribution | views::keys),
		.max = *max_element(bpm_distribution | views::keys),
		.main = max_element(bpm_distribution, [](auto const& left, auto const& right) { return left.second < right.second; })->first,
	};
	return result;
}

template<callable<void(threads::ChartLoadProgress::Type)> Func>
void calculate_metrics(Chart& chart, Func&& progress)
{
	chart.timeline.playstyle = determine_playstyle(chart.timeline.lanes);
	calculate_note_metrics(chart.timeline.lanes, chart.metadata);
	calculate_audio_metrics(Cursor{chart, true}, chart.metadata, progress);
	calculate_density_distribution(chart.metadata, chart.timeline.lanes, chart.metadata.chart_duration, 125ms, 2s, progress);
	chart.metadata.features = calculate_features(chart);
	chart.metadata.bpm_range = calculate_bpm_range(chart);
}

inline void calculate_bb(Chart& chart)
{
	chart.media.wav_bb.windows.resize(chart.metadata.audio_duration / Media::SlotBB::WindowSize + 1);

	for (auto const& lane: chart.timeline.lanes) {
		if (!lane.audible) continue;
		for (auto [idx, note]: lane.notes | views::enumerate) {
			if (chart.media.wav_slots[note.wav_slot].empty()) continue;
			// Register note audio in the structure
			auto const wav_len = dev::Audio::samples_to_ns(chart.media.wav_slots[note.wav_slot].size());
			auto const next_note_start = idx >= lane.notes.size() - 1? chart.metadata.audio_duration :
				lane.playable? lane.notes[idx + 1].timestamp : note.timestamp;
			auto const start = note.timestamp;
			auto const end = next_note_start + wav_len;
			auto const first_window = clamp<usize>(start / Media::SlotBB::WindowSize, 0zu, chart.media.wav_bb.windows.size() - 1);
			auto const last_window = clamp<usize>(end / Media::SlotBB::WindowSize + 1, 0zu, chart.media.wav_bb.windows.size() - 1);
			for (auto& window: views::iota(first_window, last_window + 1) |
				views::transform([&](auto i) -> auto& { return chart.media.wav_bb.windows[i]; })) {
				if (contains(window, note.wav_slot)) continue;
				if (window.size() == window.capacity()) {
					WARN("Unable to add sample slot to bounding box; reached limit of {}", Media::SlotBB::MaxSlots);
					continue;
				}
				window.push_back(note.wav_slot);
			}
		}
	}

	auto biggest_window = 0zu;
	for (auto const& window: chart.media.wav_bb.windows) {
		biggest_window = max(biggest_window, window.size());
	}
}

// Generate a Chart from an IR. The provided function is called to report on progress events.
template<callable<void(threads::ChartLoadProgress::Type)> Func>
auto chart_from_ir(IR const& ir, io::SongLegacy& song, Func&& progress) -> shared_ptr<Chart const>
{
	auto chart = make_shared<Chart>();
	chart->md5 = ir.get_md5();
	auto measure_rel_notes = vector<MeasureRelNote>{};
	auto measure_rel_bpms = vector<MeasureRelBPM>{};
	auto measure_lengths = vector<double>{};
	auto file_references = FileReferences{};

	measure_lengths.reserve(256); // Arbitrary; enough for most charts
	chart->media.wav_slots.resize(ir.get_wav_slot_count());
	file_references.wav.clear();
	file_references.wav.resize(ir.get_wav_slot_count());

	auto const slot_values = process_ir_headers(*chart, ir, file_references);
	measure_rel_bpms.emplace_back(NotePosition{0}, chart->metadata.bpm_range.initial, 1.0f); // Add initial BPM as the first BPM change
	process_ir_channels(ir, slot_values, measure_rel_notes, measure_rel_bpms, measure_lengths);
	stable_sort(measure_rel_bpms, [](auto const& a, auto const& b) { return a.position < b.position; });

	auto const measures = build_bpm_relative_measures(measure_lengths);
	auto beat_rel_notes = measure_rel_notes_to_beat_rel(measure_rel_notes, measures);
	generate_measure_lines(beat_rel_notes, measures);
	auto const beat_rel_bpms = measure_rel_bpms_to_beat_rel(measure_rel_bpms, measures);
	chart->timeline.bpm_sections = build_bpm_changes(beat_rel_bpms);
	auto const abs_notes = beat_rel_notes_to_abs(beat_rel_notes, beat_rel_bpms, chart->timeline.bpm_sections);
	build_lanes(*chart, abs_notes);

	load_files(*chart, song, file_references, progress);
	// Metrics require a fully loaded chart
	calculate_metrics(*chart, progress);
	calculate_bb(*chart);

	INFO("Built chart \"{}\"", chart->metadata.title);

	return chart;
}

}
