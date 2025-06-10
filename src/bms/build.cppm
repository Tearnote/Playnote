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

struct FileReferences {
	vector<optional<string>> wav;
};

using LaneBuilders = array<LaneBuilder, +Chart::LaneType::Size>;

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

[[nodiscard]] auto lufs_to_gain(double lufs) noexcept -> float
{
	constexpr auto LufsTarget = -14.0;
	auto const db_from_target = LufsTarget - lufs;
	auto const amplitude_ratio = pow(10.0, db_from_target / 20.0);
	return static_cast<float>(amplitude_ratio);
}

void calculate_metrics(Chart& chart) noexcept
{
	chart.metrics.note_count = fold_left(chart.lanes, 0u, [](auto acc, auto const& lane) noexcept {
		return acc + (lane.playable? lane.notes.size() : 0);
	});
	chart.metrics.loudness = measure_loudness(chart);
	chart.metrics.gain = lufs_to_gain(chart.metrics.loudness);
}

export template<callable<void(io::BulkRequest&)> Func>
auto chart_from_ir(IR const& ir, Func file_loader) -> shared_ptr<Chart const>
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
	file_loader(requests);

	// Metrics require a fully loaded chart
	calculate_metrics(*chart);

	INFO("Built chart \"{}\"", chart->metadata.title);

	return chart;
}

}
