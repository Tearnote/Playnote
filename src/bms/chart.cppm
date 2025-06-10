/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart, constructed from an IR. Tracks playback and gameplay.
*/

module;
#include "macros/logger.hpp"
#include "macros/assert.hpp"

export module playnote.bms.chart;

import playnote.preamble;
import playnote.logger;
import playnote.lib.pipewire;
import playnote.io.bulk_request;
import playnote.io.audio_codec;
import playnote.bms.ir;

namespace playnote::bms {

// A note of a chart with its timing information relative to BPM and measure length.
// LNs are represented as unpaired ends.
struct RelativeNote {
	struct Simple {};
	struct LNToggle {};
	using Type = variant<Simple, LNToggle>;

	Type type;
	NotePosition position;
	usize wav_slot;

	template<variant_alternative<Type> T>
	[[nodiscard]] auto type_is() const -> bool { return holds_alternative<T>(type); }

	template<variant_alternative<Type> T>
	[[nodiscard]] auto params() -> T& { return get<T>(type); }
	template<variant_alternative<Type> T>
	[[nodiscard]] auto params() const -> T const& { return get<T>(type); }
};

// A note of a chart with a definite timestamp and vertical position, ready for playback.
export struct Note {
	struct Simple {};
	struct LN {
		nanoseconds length;
		float height;
	};
	using Type = variant<
		Simple, // 0
		LN      // 1
	>;

	Type type;
	nanoseconds timestamp;
	double y_pos;
	usize wav_slot;

	template<variant_alternative<Type> T>
	[[nodiscard]] auto type_is() const -> bool { return holds_alternative<T>(type); }

	template<variant_alternative<Type> T>
	[[nodiscard]] auto params() -> T& { return get<T>(type); }
	template<variant_alternative<Type> T>
	[[nodiscard]] auto params() const -> T const& { return get<T>(type); }
};

// A column of a chart, with all the notes that will appear on it from start to end.
// Notes are expected to be sorted by timestamp from earliest.
struct Lane {
	vector<Note> notes;
	bool playable; // Are the notes for the player to hit?
};

// Factory that accumulates RelativeNotes, then converts them in bulk to a Lane.
class LaneBuilder {
public:
	LaneBuilder() = default;

	// Enqueue a RelativeNote. Notes can be enqueued in any order.
	void add_note(RelativeNote const& note) noexcept;

	// Convert enqueued notes to a Lane and clear the queue.
	auto build(float bpm, bool deduplicate = true) noexcept -> Lane;

private:
	vector<RelativeNote> notes;
	vector<RelativeNote> ln_ends;

	[[nodiscard]] static auto calculate_timestamp(NotePosition, nanoseconds measure_length) noexcept -> nanoseconds;

	static void convert_simple(vector<RelativeNote> const&, vector<Note>&, float bpm) noexcept;
	static void convert_ln(vector<RelativeNote>&, vector<Note>&, float bpm) noexcept;
	static void sort_and_deduplicate(vector<Note>&, bool deduplicate) noexcept;
};

// A list of all possible metadata about a chart.
export struct Metadata {
	using Difficulty = IR::HeaderEvent::Difficulty::Level;
	string title;
	string subtitle;
	string artist;
	string subartist;
	string genre;
	string url;
	string email;
	Difficulty difficulty = Difficulty::Unknown;

	[[nodiscard]] static auto to_str(Difficulty diff) -> string_view
	{
		switch (diff) {
		case Difficulty::Beginner: return "Beginner";
		case Difficulty::Normal: return "Normal";
		case Difficulty::Hyper: return "Hyper";
		case Difficulty::Another: return "Another";
		case Difficulty::Insane: return "Insane";
		default: return "Unknown";
		}
	}
};

// Data about a chart calculated from its contents.
export struct Metrics {
	uint32 note_count;
};

// An entire loaded chart, with all of its notes and meta information. Immutable; a chart is played
// by creating and advancing a Play from it.
export struct Chart {
	enum class LaneType: usize {
		P1_Key1,
		P1_Key2,
		P1_Key3,
		P1_Key4,
		P1_Key5,
		P1_Key6,
		P1_Key7,
		P1_KeyS,
		P2_Key1,
		P2_Key2,
		P2_Key3,
		P2_Key4,
		P2_Key5,
		P2_Key6,
		P2_Key7,
		P2_KeyS,
		BGM,
		Size,
	};

	Metadata metadata;
	Metrics metrics;
	array<Lane, +LaneType::Size> lanes;
	vector<io::AudioCodec::Output> wav_slots;
	float bpm = 130.0f; // BMS spec default
};

// Representation of a moment in chart's progress.
export class Cursor {
public:
	explicit Cursor(shared_ptr<Chart const> const& chart) noexcept;

	[[nodiscard]] auto get_chart() const noexcept -> Chart const& { return *chart; }

	void restart() noexcept;

	template<callable<void(lib::pw::Sample)> Func>
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

void LaneBuilder::add_note(RelativeNote const& note) noexcept
{
	if (note.type_is<RelativeNote::Simple>())
		notes.emplace_back(note);
	else if (note.type_is<RelativeNote::LNToggle>())
		ln_ends.emplace_back(note);
	else PANIC();
}

auto LaneBuilder::build(float bpm, bool deduplicate) noexcept -> Lane
{
	ASSERT(bpm != 0.0f);
	auto result = Lane{};

	convert_simple(notes, result.notes, bpm);
	convert_ln(ln_ends, result.notes, bpm);
	sort_and_deduplicate(result.notes, deduplicate);

	notes.clear();
	ln_ends.clear();
	return result;
}

auto LaneBuilder::calculate_timestamp(NotePosition position, nanoseconds measure_length) noexcept -> nanoseconds
{
	auto const measures = position.numerator() / position.denominator();
	auto result = measures * measure_length;
	auto const proper_numerator = position.numerator() % position.denominator();
	result += (measure_length / position.denominator()) * proper_numerator;
	return result;
}

void LaneBuilder::convert_simple(vector<RelativeNote> const& notes, vector<Note>& result, float bpm) noexcept
{
	auto const beat_duration = duration_cast<nanoseconds>(duration<double>{60.0 / bpm});
	auto const measure_duration = beat_duration * 4;
	transform(notes, back_inserter(result), [&](RelativeNote const& note) noexcept {
		ASSERT(note.type_is<RelativeNote::Simple>());
		return Note{
			.type = Note::Simple{},
			.timestamp = calculate_timestamp(note.position, measure_duration),
			.y_pos = static_cast<double>(note.position.numerator()) / static_cast<double>(note.position.denominator()),
			.wav_slot = note.wav_slot,
		};
	});
}

void LaneBuilder::convert_ln(vector<RelativeNote>& ln_ends, vector<Note>& result, float bpm) noexcept
{
	auto const beat_duration = duration_cast<nanoseconds>(duration<double>{60.0 / bpm});
	auto const measure_duration = beat_duration * 4;
	stable_sort(ln_ends, [](auto const& a, auto const& b) noexcept { return a.position < b.position; });
	if (ln_ends.size() % 2 != 0) {
		WARN("Unpaired LN end found; chart is most likely invalid");
		ln_ends.pop_back();
	}
	transform(ln_ends | views::chunk(2), back_inserter(result), [&](auto ends) noexcept {
		auto const ln_length = ends[1].position - ends[0].position;
		return Note{
			.type = Note::LN{
				.length = calculate_timestamp(ln_length, measure_duration),
				.height = static_cast<float>(ln_length.numerator()) / static_cast<float>(ln_length.denominator()),
			},
			.timestamp = calculate_timestamp(ends[0].position, measure_duration),
			.y_pos = static_cast<double>(ends[0].position.numerator()) / static_cast<double>(ends[0].position.denominator()),
			.wav_slot = ends[0].wav_slot,
		};
	});
}

void LaneBuilder::sort_and_deduplicate(vector<Note>& result, bool deduplicate) noexcept
{
	if (!deduplicate) {
		stable_sort(result, [](auto const& a, auto const& b) noexcept {
			return a.timestamp < b.timestamp;
		});
		return;
	}
	// std::unique keeps the first of the two duplicate elements while we want to keep the second,
	// so the range is reversed first
	stable_sort(result, [](auto const& a, auto const& b) noexcept {
		return a.timestamp > b.timestamp; // Reverse sort
	});
	auto removed = unique(result, [](auto const& a, auto const& b) noexcept {
		return a.timestamp == b.timestamp;
	});
	auto removed_count = removed.size();
	result.erase(removed.begin(), removed.end());
	if (removed_count) INFO("Removed {} duplicate notes", removed_count);
	reverse(result); // Reverse back
}

template<callable<void(lib::pw::Sample)> Func>
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

void Cursor::restart() noexcept
{
	sample_progress = 0zu;
	notes_judged = 0zu;
	for (auto& lane: lane_progress) lane.restart();
	for (auto& slot: wav_slot_progress) slot.playback_pos = WavSlotProgress::Stopped;
}

Cursor::Cursor(shared_ptr<Chart const> const& chart) noexcept:
	chart{chart}
{
	wav_slot_progress.resize(chart->wav_slots.size());
}

auto Cursor::samples_to_ns(usize samples) noexcept -> nanoseconds
{
	auto const sampling_rate = io::AudioCodec::sampling_rate;
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	auto const whole_seconds = samples / sampling_rate;
	auto const remainder = samples % sampling_rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

struct FileReferences {
	vector<optional<string>> wav;
};

using LaneBuilders = array<LaneBuilder, +Chart::LaneType::Size>;

auto channel_to_note_type(IR::ChannelEvent::Type ch) noexcept -> RelativeNote::Type
{
	using ChannelType = IR::ChannelEvent::Type;
	if (ch >= ChannelType::BGM && ch <= ChannelType::Note_P2_KeyS) return RelativeNote::Simple{};
	if (ch >= ChannelType::Note_P1_Key1_LN && ch <= ChannelType::Note_P2_KeyS_LN) return RelativeNote::LNToggle{};
	PANIC();
}

auto channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> Chart::LaneType
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

void process_ir_headers(Chart& chart, IR const& ir, FileReferences& file_references)
{
	ir.each_header_event([&](IR::HeaderEvent const& event) noexcept {
		visit(visitor {
			[&](IR::HeaderEvent::Title* params) noexcept { chart.metadata.title = params->title; },
			[&](IR::HeaderEvent::Subtitle* params) noexcept { chart.metadata.subtitle = params->subtitle; },
			[&](IR::HeaderEvent::Artist* params) noexcept { chart.metadata.artist = params->artist; },
			[&](IR::HeaderEvent::Subartist* params) noexcept { chart.metadata.subartist = params->subartist; },
			[&](IR::HeaderEvent::Genre* params) noexcept { chart.metadata.genre = params->genre; },
			[&](IR::HeaderEvent::URL* params) noexcept { chart.metadata.url = params->url; },
			[&](IR::HeaderEvent::Email* params) noexcept { chart.metadata.email = params->email; },
			[&](IR::HeaderEvent::Difficulty* params) noexcept { chart.metadata.difficulty = params->level; },
			[&](IR::HeaderEvent::BPM* params) noexcept { chart.bpm = params->bpm; },
			[&](IR::HeaderEvent::WAV* params) noexcept { file_references.wav[params->slot] = params->name; },
			[](auto&&) noexcept {}
		}, event.params);
	});
}

void process_ir_channels(IR const& ir, LaneBuilders& lane_builders)
{
	ir.each_channel_event([&](IR::ChannelEvent const& event) noexcept {
		auto const lane_id = channel_to_lane(event.type);
		if (lane_id == Chart::LaneType::Size) return;
		lane_builders[+lane_id].add_note(RelativeNote{
			.type = channel_to_note_type(event.type),
			.position = event.position,
			.wav_slot = event.slot,
		});
	});
}

void build_lanes(Chart& chart, LaneBuilders& lane_builders) noexcept
{
	for (auto [idx, lane]: chart.lanes | views::enumerate) {
		auto const is_bgm = idx == +Chart::LaneType::BGM;
		lane = lane_builders[idx].build(chart.bpm, !is_bgm);
		if (!is_bgm) lane.playable = true;
	}
}

void calculate_metrics(Chart& chart) noexcept
{
	chart.metrics.note_count = fold_left(chart.lanes, 0u, [](auto acc, auto const& lane) noexcept {
		return acc + (lane.playable? lane.notes.size() : 0);
	});
}

export auto from_ir(IR const& ir) -> pair<shared_ptr<Chart const>, io::BulkRequest>
{
	static constexpr auto AudioExtensions = {"wav"sv, "ogg"sv, "mp3"sv, "flac"sv, "opus"sv};

	auto chart = make_shared<Chart>();
	LaneBuilders lane_builders;
	FileReferences file_references;

	auto const domain = ir.get_path().parent_path();
	chart->wav_slots.resize(ir.get_wav_slot_count());
	file_references.wav.clear();
	file_references.wav.resize(ir.get_wav_slot_count());

	process_ir_headers(*chart, ir, file_references);
	process_ir_channels(ir, lane_builders);
	build_lanes(*chart, lane_builders);
	calculate_metrics(*chart);

	auto needed_slots = vector<bool>{};
	needed_slots.resize(chart->wav_slots.size(), false);

	// Mark used slots
	for (auto const& lane: chart->lanes) {
		for (auto const& note: lane.notes) {
			needed_slots[note.wav_slot] = true;
		}
	}

	// Enqueue file requests for used slots
	auto requests = io::BulkRequest{domain};
	for (auto const& [needed, request, wav_slot]: views::zip(needed_slots, file_references.wav, chart->wav_slots)) {
		if (!needed || !request) continue;
		requests.enqueue<io::AudioCodec>(wav_slot, *request, AudioExtensions, false);
	}

	INFO("Built chart \"{}\"", chart->metadata.title);

	return {chart, requests};
}

}
