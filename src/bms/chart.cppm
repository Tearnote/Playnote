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

struct RelativeNote {
	struct Simple {};
	struct LNToggle {};
	using Type = variant<Simple, LNToggle>;

	Type type;
	NotePosition position;
	usize wav_slot;
};

export struct Note {
	struct Simple {};
	struct LN {
		nanoseconds length;
		float height;
	};
	using Type = variant<Simple, LN>;

	Type type;
	nanoseconds timestamp;
	double y_pos;
	usize wav_slot;
};

enum class NoteType: usize {
	Simple = 0,
	LN = 1,
};

struct Lane {
	vector<Note> notes;
	usize next_note;
	bool ln_active;
};

class LaneBuilder {
public:
	LaneBuilder() = default;

	void add_note(RelativeNote const& note) noexcept;

	auto build(float bpm, bool deduplicate = true) noexcept -> Lane;

private:
	vector<RelativeNote> notes;
	vector<RelativeNote> ln_ends;

	[[nodiscard]] static auto calculate_timestamp(NotePosition, nanoseconds measure_length) noexcept -> nanoseconds;

	static void convert_simple(vector<RelativeNote> const&, vector<Note>&, float bpm) noexcept;
	static void convert_ln(vector<RelativeNote>&, vector<Note>&, float bpm) noexcept;
	static void sort_and_deduplicate(vector<Note>&, bool deduplicate) noexcept;
};

export class ChartBuilder;

export class Chart {
public:
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

	void restart() noexcept;
	[[nodiscard]] auto at_start() const noexcept -> bool { return progress == 0; }

	template<callable<void(lib::pw::Sample)> Func>
	auto advance_one_sample(Func&& func) noexcept -> bool;

	template<callable<void(Note const&, LaneType, float)> Func>
	void upcoming_notes(float max_units, Func&& func) const noexcept;

private:
	friend class ChartBuilder;

	struct WavSlot {
		static constexpr auto Stopped = -1zu;
		io::AudioCodec::Output audio;
		usize playback_pos;
	};

	array<Lane, +LaneType::Size> lanes;

	usize progress = 0zu;
	usize note_count = 0zu;
	usize notes_hit = 0zu;

	vector<optional<WavSlot>> wav_slots;

	string title;
	string artist;
	float bpm = 130.0f; // BMS spec default

	Chart() = default;

	[[nodiscard]] static auto progress_to_ns(usize) noexcept -> nanoseconds;

};

class ChartBuilder {
public:
	ChartBuilder() = default;

	auto from_ir(IR const&) -> Chart;

	[[nodiscard]] auto make_file_requests(Chart&) noexcept -> io::BulkRequest;

private:
	struct FileReferences {
		vector<optional<string>> wav;
	};

	static constexpr auto AudioExtensions = {"wav"sv, "ogg"sv, "mp3"sv, "flac"sv, "opus"sv};
	fs::path domain;
	array<LaneBuilder, +Chart::LaneType::Size> lane_builders;
	FileReferences file_references;

	[[nodiscard]] static auto channel_to_note_type(IR::ChannelEvent::Type) noexcept -> RelativeNote::Type;
	[[nodiscard]] static auto channel_to_lane(IR::ChannelEvent::Type) noexcept -> Chart::LaneType;

	void process_ir_headers(Chart&, IR const&);
	void process_ir_channels(Chart&, IR const&);
};

void LaneBuilder::add_note(RelativeNote const& note) noexcept
{
	if (holds_alternative<RelativeNote::Simple>(note.type))
		notes.emplace_back(note);
	else if (holds_alternative<RelativeNote::LNToggle>(note.type))
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
	transform(notes, back_inserter(result), [&](auto const& note) noexcept {
		ASSERT(holds_alternative<RelativeNote::Simple>(note.type));
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
auto Chart::advance_one_sample(Func&& func) noexcept -> bool
{
	auto chart_ended = (notes_hit >= note_count);
	progress += 1;
	auto const progress_ns = progress_to_ns(progress);
	for (auto& lane: lanes) {
		if (lane.next_note >= lane.notes.size()) continue;
		auto const& note = lane.notes[lane.next_note];
		if (progress_ns >= note.timestamp) {
			if (note.type.index() == +NoteType::Simple || (note.type.index() == +NoteType::LN && progress_ns >= note.timestamp + get<Note::LN>(note.type).length)) {
				lane.next_note += 1;
				if (static_cast<LaneType>(&lane - &lanes.front()) != LaneType::BGM) notes_hit += 1;
				if (note.type.index() == +NoteType::LN) {
					lane.ln_active = false;
					continue;
				}
			}
			if (!wav_slots[note.wav_slot]) continue;
			if (note.type.index() == +NoteType::Simple || (note.type.index() == +NoteType::LN && !lane.ln_active)) {
				wav_slots[note.wav_slot]->playback_pos = 0;
				if (note.type.index() == +NoteType::LN) lane.ln_active = true;
			}
		}
	}

	for (auto& slot: wav_slots) {
		if (!slot) continue;
		if (slot->playback_pos == WavSlot::Stopped) continue;
		auto const result = slot->audio[slot->playback_pos];
		slot->playback_pos += 1;
		if (slot->playback_pos >= slot->audio.size())
			slot->playback_pos = WavSlot::Stopped;
		func(result);
		chart_ended = false;
	}

	return chart_ended;
}

template<callable<void(Note const&, Chart::LaneType, float)> Func>
void Chart::upcoming_notes(float max_units, Func&& func) const noexcept
{
	auto const beat_duration = duration_cast<nanoseconds>(duration<double>{60.0 / bpm});
	auto const measure_duration = beat_duration * 4;
	auto const current_y = duration_cast<duration<double>>(progress_to_ns(progress)) / measure_duration;
	for (auto& lane: span{
		lanes.begin() + +LaneType::P1_Key1,
		lanes.begin() + +LaneType::P2_KeyS + 1
	}) {
		for (auto& note: span{
			lane.notes.begin() + lane.next_note,
			lane.notes.end()
		}) {
			auto const distance = note.y_pos - current_y;
			if (distance > max_units) break;
			func(note, static_cast<LaneType>(&lane - &lanes.front()), distance);
		}
	}
}

void Chart::restart() noexcept
{
	for (auto& slot: wav_slots) {
		if (!slot) continue;
		slot->playback_pos = WavSlot::Stopped;
	}
	progress = 0zu;
	notes_hit = 0zu;
	for (auto& lane: lanes) {
		lane.next_note = 0;
	}
}

auto Chart::progress_to_ns(usize samples) noexcept -> nanoseconds
{
	auto const sampling_rate = io::AudioCodec::sampling_rate;
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	auto const whole_seconds = samples / sampling_rate;
	auto const remainder = samples % sampling_rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
}

auto ChartBuilder::from_ir(IR const& ir) -> Chart
{
	auto chart = Chart{};

	domain = ir.get_path().parent_path();
	chart.wav_slots.resize(ir.get_wav_slot_count());
	file_references.wav.clear();
	file_references.wav.resize(ir.get_wav_slot_count());

	process_ir_headers(chart, ir);
	process_ir_channels(chart, ir);

	for (auto const i: views::iota(0u, +Chart::LaneType::Size))
		chart.lanes[i] = move(lane_builders[i].build(chart.bpm, i != +Chart::LaneType::BGM));

	chart.note_count = fold_left(span{chart.lanes.begin(), chart.lanes.begin() + +Chart::LaneType::BGM}, 0u, [](auto acc, auto const& lane) noexcept {
		return acc + lane.notes.size();
	});

	INFO("Built chart \"{}\"", chart.title);

	return chart;
}

auto ChartBuilder::make_file_requests(Chart& chart) noexcept -> io::BulkRequest
{
	auto needed_slots = vector<bool>{};
	needed_slots.resize(chart.wav_slots.size(), false);

	// Mark used slots
	for (auto const& lane: chart.lanes) {
		for (auto const& note: lane.notes) {
			needed_slots[note.wav_slot] = true;
		}
	}

	// Enqueue file requests for used slots
	auto requests = io::BulkRequest{domain};
	for (auto const& [needed, request, wav_slot]: views::zip(needed_slots, file_references.wav, chart.wav_slots)) {
		if (!needed || !request) continue;
		wav_slot = Chart::WavSlot{ .playback_pos = Chart::WavSlot::Stopped };
		requests.enqueue<io::AudioCodec>(wav_slot->audio, *request, AudioExtensions, false);
	}
	return requests;
}

auto ChartBuilder::channel_to_note_type(IR::ChannelEvent::Type ch) noexcept -> RelativeNote::Type
{
	using ChannelType = IR::ChannelEvent::Type;
	if (ch >= ChannelType::BGM && ch <= ChannelType::Note_P2_KeyS) return RelativeNote::Simple{};
	if (ch >= ChannelType::Note_P1_Key1_LN && ch <= ChannelType::Note_P2_KeyS_LN) return RelativeNote::LNToggle{};
	PANIC();
}

auto ChartBuilder::channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> Chart::LaneType
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM: return Chart::LaneType::BGM;
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

void ChartBuilder::process_ir_headers(Chart& chart, IR const& ir)
{
	ir.each_header_event([&](IR::HeaderEvent const& event) noexcept {
		event.params.visit(visitor {
			[&](IR::HeaderEvent::Title* title_params) noexcept { chart.title = title_params->title; },
			[&](IR::HeaderEvent::Artist* artist_params) noexcept { chart.artist = artist_params->artist; },
			[&](IR::HeaderEvent::BPM* bpm_params) noexcept { chart.bpm = bpm_params->bpm; },
			[&](IR::HeaderEvent::WAV* wav_params) noexcept {
				file_references.wav[wav_params->slot] = wav_params->name;
			},
			[](auto&&) noexcept {}
		});
	});
}

void ChartBuilder::process_ir_channels(Chart& chart, IR const& ir)
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

}
