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

export class ChartBuilder;

export class Chart {
public:
	struct Note {
		enum class Type {
			Note,
			LN,
		};
		using Position = NotePosition;
		using Length = NotePosition;
		Type type;
		Position position;
		nanoseconds timestamp;
		Length length; // if LN
		nanoseconds length_ns;
		usize slot;
	};
	struct Lane {
		enum class Type: usize {
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

		vector<Note> notes;
		usize next_note;
		bool ln_active;
	};

	using NoteType = Note::Type;
	using LaneType = Lane::Type;

	void restart() noexcept;
	[[nodiscard]] auto at_start() const noexcept -> bool { return progress == 0; }

	template<callable<void(lib::pw::Sample)> Func>
	auto advance_one_sample(Func&& func) noexcept -> bool;

	template<callable<void(Note const&, LaneType, nanoseconds)> Func>
	void upcoming_notes(nanoseconds max_distance, Func&& func) const noexcept;

private:
	friend class ChartBuilder;

	struct WavSlot {
		static constexpr auto Stopped = -1zu;
		io::AudioCodec::Output audio;
		usize playback_pos;
	};

	array<Lane, to_underlying(Lane::Type::Size)> lanes;

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
	FileReferences file_references;

	[[nodiscard]] static auto channel_to_note_type(IR::ChannelEvent::Type) noexcept -> Chart::NoteType;
	[[nodiscard]] static auto note_channel_to_lane(IR::ChannelEvent::Type) noexcept -> Chart::LaneType;
	[[nodiscard]] static auto ln_channel_to_lane(IR::ChannelEvent::Type) noexcept -> Chart::LaneType;
	[[nodiscard]] static auto calculate_timestamp(NotePosition, nanoseconds measure) noexcept -> nanoseconds;

	void add_note(Chart&, IR::ChannelEvent const&) noexcept;
	void add_ln_end(vector<vector<Chart::Note>>&, IR::ChannelEvent const&) noexcept;
	void ln_ends_to_lns(Chart&, vector<vector<Chart::Note>>&) noexcept;

	void process_ir_headers(Chart&, IR const&);
	void process_ir_channels(Chart&, IR const&);
	void sort_lanes(Chart&) noexcept;
	void calculate_object_timestamps(Chart&) noexcept;
};

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
			if (note.type == NoteType::Note || (note.type == NoteType::LN && progress_ns >= note.timestamp + note.length_ns)) {
				lane.next_note += 1;
				if (static_cast<LaneType>(&lane - &lanes.front()) != LaneType::BGM) notes_hit += 1;
				if (note.type == NoteType::LN) {
					lane.ln_active = false;
					continue;
				}
			}
			if (!wav_slots[note.slot]) continue;
			if (note.type == NoteType::Note || (note.type == NoteType::LN && !lane.ln_active)) {
				wav_slots[note.slot]->playback_pos = 0;
				if (note.type == NoteType::LN) lane.ln_active = true;
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

template<callable<void(Chart::Note const&, Chart::LaneType, nanoseconds)> Func>
void Chart::upcoming_notes(nanoseconds max_distance, Func&& func) const noexcept
{
	for (auto& lane: span{
		lanes.begin() + to_underlying(Lane::Type::P1_Key1),
		lanes.begin() + to_underlying(Lane::Type::P2_KeyS) + 1
	}) {
		for (auto& note: span{
			lane.notes.begin() + lane.next_note,
			lane.notes.end()
		}) {
			auto const distance = note.timestamp - progress_to_ns(progress);
			if (distance > max_distance) break;
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

	sort_lanes(chart);
	calculate_object_timestamps(chart);

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
			needed_slots[note.slot] = true;
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

auto ChartBuilder::channel_to_note_type(IR::ChannelEvent::Type ch) noexcept -> Chart::NoteType
{
	using ChannelType = IR::ChannelEvent::Type;
	if (ch >= ChannelType::BGM && ch <= ChannelType::Note_P2_KeyS) return Chart::NoteType::Note;
	if (ch >= ChannelType::Note_P1_Key1_LN && ch <= ChannelType::Note_P2_KeyS_LN) return Chart::NoteType::LN;
	PANIC();
}

auto ChartBuilder::note_channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> Chart::Lane::Type
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM: return Chart::Lane::Type::BGM;
	case IR::ChannelEvent::Type::Note_P1_Key1: return Chart::Lane::Type::P1_Key1;
	case IR::ChannelEvent::Type::Note_P1_Key2: return Chart::Lane::Type::P1_Key2;
	case IR::ChannelEvent::Type::Note_P1_Key3: return Chart::Lane::Type::P1_Key3;
	case IR::ChannelEvent::Type::Note_P1_Key4: return Chart::Lane::Type::P1_Key4;
	case IR::ChannelEvent::Type::Note_P1_Key5: return Chart::Lane::Type::P1_Key5;
	case IR::ChannelEvent::Type::Note_P1_Key6: return Chart::Lane::Type::P1_Key6;
	case IR::ChannelEvent::Type::Note_P1_Key7: return Chart::Lane::Type::P1_Key7;
	case IR::ChannelEvent::Type::Note_P1_KeyS: return Chart::Lane::Type::P1_KeyS;
	case IR::ChannelEvent::Type::Note_P2_Key1: return Chart::Lane::Type::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2: return Chart::Lane::Type::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3: return Chart::Lane::Type::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4: return Chart::Lane::Type::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5: return Chart::Lane::Type::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6: return Chart::Lane::Type::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7: return Chart::Lane::Type::P2_Key7;
	case IR::ChannelEvent::Type::Note_P2_KeyS: return Chart::Lane::Type::P2_KeyS;
	default: return Chart::Lane::Type::Size;
	}
}

auto ChartBuilder::ln_channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> Chart::LaneType
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM: return Chart::Lane::Type::BGM;
	case IR::ChannelEvent::Type::Note_P1_Key1_LN: return Chart::Lane::Type::P1_Key1;
	case IR::ChannelEvent::Type::Note_P1_Key2_LN: return Chart::Lane::Type::P1_Key2;
	case IR::ChannelEvent::Type::Note_P1_Key3_LN: return Chart::Lane::Type::P1_Key3;
	case IR::ChannelEvent::Type::Note_P1_Key4_LN: return Chart::Lane::Type::P1_Key4;
	case IR::ChannelEvent::Type::Note_P1_Key5_LN: return Chart::Lane::Type::P1_Key5;
	case IR::ChannelEvent::Type::Note_P1_Key6_LN: return Chart::Lane::Type::P1_Key6;
	case IR::ChannelEvent::Type::Note_P1_Key7_LN: return Chart::Lane::Type::P1_Key7;
	case IR::ChannelEvent::Type::Note_P1_KeyS_LN: return Chart::Lane::Type::P1_KeyS;
	case IR::ChannelEvent::Type::Note_P2_Key1_LN: return Chart::Lane::Type::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2_LN: return Chart::Lane::Type::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3_LN: return Chart::Lane::Type::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4_LN: return Chart::Lane::Type::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5_LN: return Chart::Lane::Type::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6_LN: return Chart::Lane::Type::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7_LN: return Chart::Lane::Type::P2_Key7;
	case IR::ChannelEvent::Type::Note_P2_KeyS_LN: return Chart::Lane::Type::P2_KeyS;
	default: return Chart::Lane::Type::Size;
	}
}

auto ChartBuilder::calculate_timestamp(NotePosition position, nanoseconds measure) noexcept -> nanoseconds
{
	auto const measures = position.numerator() / position.denominator();
	auto result = measures * measure;
	auto const proper_numerator = position.numerator() % position.denominator();
	result += (measure / position.denominator()) * proper_numerator;
	return result;
}

void ChartBuilder::add_note(Chart& chart, IR::ChannelEvent const& event) noexcept
{
	auto const lane_id = note_channel_to_lane(event.type);
	if (lane_id == Chart::Lane::Type::Size) return;
	chart.lanes[to_underlying(lane_id)].notes.emplace_back(Chart::Note{
		.type = Chart::NoteType::Note,
		.position = event.position,
		.slot = event.slot,
	});
	if (lane_id != Chart::LaneType::BGM) chart.note_count += 1;
}

void ChartBuilder::add_ln_end(vector<vector<Chart::Note>>& ln_ends, IR::ChannelEvent const& event) noexcept
{
	auto const lane_id = ln_channel_to_lane(event.type);
	if (lane_id == Chart::Lane::Type::Size) return;
	ln_ends[to_underlying(lane_id)].emplace_back(Chart::Note{
		.type = Chart::NoteType::LN,
		.position = event.position,
		.slot = event.slot,
	});
}

void ChartBuilder::ln_ends_to_lns(Chart& chart, vector<vector<Chart::Note>>& ln_ends) noexcept
{
	for (auto [ln_lane, lane]: views::zip(ln_ends, chart.lanes)) {
		stable_sort(ln_lane, [](auto const& a, auto const& b) noexcept {
			return a.position < b.position;
		});

		for (auto const& pair: ln_lane | views::chunk(2)) {
			if (pair.size() < 2) return;
			lane.notes.emplace_back(Chart::Note{
				.type = Chart::NoteType::LN,
				.position = pair[0].position,
				.length = pair[1].position - pair[0].position,
				.slot = pair[0].slot,
			});
			chart.note_count += 1;
		}
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
	auto ln_ends = vector<vector<Chart::Note>>{};
	ln_ends.resize(to_underlying(Chart::LaneType::Size) - 1);

	ir.each_channel_event([&](IR::ChannelEvent const& event) noexcept {
		auto const note_type = channel_to_note_type(event.type);
		if (note_type == Chart::NoteType::Note) add_note(chart, event);
		else if (note_type == Chart::NoteType::LN) add_ln_end(ln_ends, event);
		else PANIC();
	});

	ln_ends_to_lns(chart, ln_ends);
}

void ChartBuilder::sort_lanes(Chart& chart) noexcept
{
	auto removed_count = 0zu;
	for (auto& lane: chart.lanes) {
		stable_sort(lane.notes, [](auto const& a, auto const& b) noexcept {
			return a.position < b.position;
		});
		if (&lane - &chart.lanes.front() == to_underlying(Chart::LaneType::BGM)) continue;
		auto removed = unique(lane.notes, [](auto const& a, auto const& b) noexcept {
			return a.position == b.position;
		});
		lane.notes.erase(removed.begin(), removed.end());
		removed_count += removed.size();
	}
	if (removed_count) INFO("Removed {} duplicate notes", removed_count);
}

void ChartBuilder::calculate_object_timestamps(Chart& chart) noexcept
{
	auto const beat_duration = duration_cast<nanoseconds>(duration<double>{60.0 / chart.bpm});
	auto const measure_duration = beat_duration * 4;
	for (auto& lane: chart.lanes) {
		for (auto& note: lane.notes) {
			note.timestamp = calculate_timestamp(note.position, measure_duration);
			note.length_ns = calculate_timestamp(note.length, measure_duration);
		}
	}
}

}
