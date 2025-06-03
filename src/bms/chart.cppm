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
	};

	using NoteType = Note::Type;
	using LaneType = Lane::Type;

	static auto from_ir(IR const&) noexcept -> Chart;

	void restart() noexcept;

	[[nodiscard]] auto make_file_requests() noexcept -> io::BulkRequest;

	template<callable<void(lib::pw::Sample)> Func>
	void advance_one_sample(Func&& func) noexcept;

	template<callable<void(Note const&, LaneType, nanoseconds)> Func>
	void upcoming_notes(nanoseconds max_distance, Func&& func) const noexcept;

private:
	static constexpr auto AudioExtensions = {"wav"sv, "ogg"sv, "mp3"sv, "flac"sv, "opus"sv};

	struct WavSlot {
		static constexpr auto Stopped = -1zu;
		io::AudioCodec::Output audio;
		usize playback_pos;
	};
	struct FileReferences {
		vector<optional<string>> wav;
	};

	array<Lane, to_underlying(Lane::Type::Size)> lanes;

	usize progress = 0zu;

	vector<optional<WavSlot>> wav_slots;

	FileReferences file_references;

	Chart() = default; // Can only be created with factory methods

	fs::path domain;
	string title;
	string artist;
	float bpm = 130.0f; // BMS spec default

	[[nodiscard]] static auto channel_to_note_type(IR::ChannelEvent::Type) noexcept -> NoteType;
	[[nodiscard]] static auto note_channel_to_lane(IR::ChannelEvent::Type) noexcept -> LaneType;
	[[nodiscard]] static auto ln_channel_to_lane(IR::ChannelEvent::Type) noexcept -> LaneType;

	[[nodiscard]] static auto progress_to_ns(usize) noexcept -> nanoseconds;
	[[nodiscard]] static auto calculate_timestamp(NotePosition, nanoseconds measure) noexcept -> nanoseconds;

	void add_note(IR::ChannelEvent const&) noexcept;
	void add_ln_end(vector<vector<Note>>&, IR::ChannelEvent const&) noexcept;
	void ln_ends_to_lns(vector<vector<Note>>&) noexcept;

	void process_ir_headers(IR const&) noexcept;
	void process_ir_channels(IR const&) noexcept;
	void sort_lanes() noexcept;
	void calculate_object_timestamps() noexcept;

};

template<callable<void(lib::pw::Sample)> Func>
void Chart::advance_one_sample(Func&& func) noexcept
{
	progress += 1;
	auto const progress_ns = progress_to_ns(progress);
	for (auto& lane: lanes) {
		if (lane.next_note >= lane.notes.size()) continue;
		auto const& note = lane.notes[lane.next_note];
		if (progress_ns >= note.timestamp) {
			lane.next_note += 1;
			if (!wav_slots[note.slot]) continue;
			wav_slots[note.slot]->playback_pos = 0;
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
	}
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

auto Chart::from_ir(IR const& ir) noexcept -> Chart
{
	auto chart = Chart{};

	chart.domain = ir.get_path().parent_path();
	chart.wav_slots.resize(ir.get_wav_slot_count());
	chart.file_references.wav.resize(ir.get_wav_slot_count());

	chart.process_ir_headers(ir);
	chart.process_ir_channels(ir);

	chart.sort_lanes();
	chart.calculate_object_timestamps();

	INFO("Built chart \"{}\"", chart.title);

	return chart;
}

void Chart::restart() noexcept
{
	for (auto& slot: wav_slots) {
		if (!slot) continue;
		slot->playback_pos = WavSlot::Stopped;
	}
	progress = 0zu;
	for (auto& lane: lanes) {
		lane.next_note = 0;
	}
}

auto Chart::make_file_requests() noexcept -> io::BulkRequest
{
	auto needed_slots = vector<bool>{};
	needed_slots.resize(wav_slots.size(), false);

	// Mark used slots
	for (auto const& lane: lanes) {
		for (auto const& note: lane.notes) {
			needed_slots[note.slot] = true;
		}
	}

	// Enqueue file requests for used slots
	auto requests = io::BulkRequest{domain};
	for (auto const& [needed, request, wav_slot]: views::zip(needed_slots, file_references.wav, wav_slots)) {
		if (!needed || !request) continue;
		wav_slot = WavSlot{ .playback_pos = WavSlot::Stopped };
		requests.enqueue<io::AudioCodec>(wav_slot->audio, *request, AudioExtensions, false);
	}
	return requests;
}

auto Chart::channel_to_note_type(IR::ChannelEvent::Type ch) noexcept -> NoteType
{
	using ChannelType = IR::ChannelEvent::Type;
	auto const ch_val = to_underlying(ch);
	if (ch >= ChannelType::BGM && ch <= ChannelType::Note_P2_KeyS) return NoteType::Note;
	if (ch >= ChannelType::Note_P1_Key1_LN && ch <= ChannelType::Note_P2_KeyS_LN) return NoteType::LN;
	PANIC();
}

auto Chart::note_channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> Lane::Type
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM: return Lane::Type::BGM;
	case IR::ChannelEvent::Type::Note_P1_Key1: return Lane::Type::P1_Key1;
	case IR::ChannelEvent::Type::Note_P1_Key2: return Lane::Type::P1_Key2;
	case IR::ChannelEvent::Type::Note_P1_Key3: return Lane::Type::P1_Key3;
	case IR::ChannelEvent::Type::Note_P1_Key4: return Lane::Type::P1_Key4;
	case IR::ChannelEvent::Type::Note_P1_Key5: return Lane::Type::P1_Key5;
	case IR::ChannelEvent::Type::Note_P1_Key6: return Lane::Type::P1_Key6;
	case IR::ChannelEvent::Type::Note_P1_Key7: return Lane::Type::P1_Key7;
	case IR::ChannelEvent::Type::Note_P1_KeyS: return Lane::Type::P1_KeyS;
	case IR::ChannelEvent::Type::Note_P2_Key1: return Lane::Type::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2: return Lane::Type::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3: return Lane::Type::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4: return Lane::Type::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5: return Lane::Type::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6: return Lane::Type::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7: return Lane::Type::P2_Key7;
	case IR::ChannelEvent::Type::Note_P2_KeyS: return Lane::Type::P2_KeyS;
	default: return Lane::Type::Size;
	}
}

auto Chart::ln_channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> LaneType
{
	switch (ch) {
	case IR::ChannelEvent::Type::BGM: return Lane::Type::BGM;
	case IR::ChannelEvent::Type::Note_P1_Key1_LN: return Lane::Type::P1_Key1;
	case IR::ChannelEvent::Type::Note_P1_Key2_LN: return Lane::Type::P1_Key2;
	case IR::ChannelEvent::Type::Note_P1_Key3_LN: return Lane::Type::P1_Key3;
	case IR::ChannelEvent::Type::Note_P1_Key4_LN: return Lane::Type::P1_Key4;
	case IR::ChannelEvent::Type::Note_P1_Key5_LN: return Lane::Type::P1_Key5;
	case IR::ChannelEvent::Type::Note_P1_Key6_LN: return Lane::Type::P1_Key6;
	case IR::ChannelEvent::Type::Note_P1_Key7_LN: return Lane::Type::P1_Key7;
	case IR::ChannelEvent::Type::Note_P1_KeyS_LN: return Lane::Type::P1_KeyS;
	case IR::ChannelEvent::Type::Note_P2_Key1_LN: return Lane::Type::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2_LN: return Lane::Type::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3_LN: return Lane::Type::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4_LN: return Lane::Type::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5_LN: return Lane::Type::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6_LN: return Lane::Type::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7_LN: return Lane::Type::P2_Key7;
	case IR::ChannelEvent::Type::Note_P2_KeyS_LN: return Lane::Type::P2_KeyS;
	default: return Lane::Type::Size;
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

auto Chart::calculate_timestamp(NotePosition position, nanoseconds measure) noexcept -> nanoseconds
{
	auto const measures = position.numerator() / position.denominator();
	auto result = measures * measure;
	auto const proper_numerator = position.numerator() % position.denominator();
	result += (measure / position.denominator()) * proper_numerator;
	return result;
}

void Chart::add_note(IR::ChannelEvent const& event) noexcept
{
	auto const lane_id = note_channel_to_lane(event.type);
	if (lane_id == Lane::Type::Size) return;
	lanes[to_underlying(lane_id)].notes.emplace_back(Note{
		.type = NoteType::Note,
		.position = event.position,
		.slot = event.slot,
	});
}

void Chart::add_ln_end(vector<vector<Note>>& ln_ends, IR::ChannelEvent const& event) noexcept
{
	auto const lane_id = ln_channel_to_lane(event.type);
	if (lane_id == Lane::Type::Size) return;
	ln_ends[to_underlying(lane_id)].emplace_back(Note{
		.type = NoteType::LN,
		.position = event.position,
		.slot = event.slot,
	});
}

void Chart::ln_ends_to_lns(vector<vector<Note>>& ln_ends) noexcept
{
	for (auto [ln_lane, lane]: views::zip(ln_ends, lanes)) {
		sort(ln_lane, [](auto const& a, auto const& b) noexcept {
			return a.position < b.position;
		});

		for (auto const& pair: ln_lane | views::chunk(2)) {
			if (pair.size() < 2) return;
			lane.notes.emplace_back(Note{
				.type = NoteType::LN,
				.position = pair[0].position,
				.length = pair[1].position - pair[0].position,
				.slot = pair[0].slot,
			});
		}
	}
}

void Chart::process_ir_headers(IR const& ir) noexcept
{
	ir.each_header_event([&](IR::HeaderEvent const& event) noexcept {
		event.params.visit(visitor {
			[&](IR::HeaderEvent::Title* title_params) noexcept { title = title_params->title; },
			[&](IR::HeaderEvent::Artist* artist_params) noexcept { artist = artist_params->artist; },
			[&](IR::HeaderEvent::BPM* bpm_params) noexcept { bpm = bpm_params->bpm; },
			[&](IR::HeaderEvent::WAV* wav_params) noexcept {
				file_references.wav[wav_params->slot] = wav_params->name;
			},
			[](auto&&) noexcept {}
		});
	});
}

void Chart::process_ir_channels(IR const& ir) noexcept
{
	auto ln_ends = vector<vector<Note>>{};
	ln_ends.resize(to_underlying(LaneType::Size) - 1);

	ir.each_channel_event([&](IR::ChannelEvent const& event) noexcept {
		auto note_type = channel_to_note_type(event.type);
		if (note_type == NoteType::Note) add_note(event);
		else if (note_type == NoteType::LN) add_ln_end(ln_ends, event);
		else PANIC();
	});

	ln_ends_to_lns(ln_ends);
}

void Chart::sort_lanes() noexcept
{
	for (auto& lane: lanes) {
		sort(lane.notes, [](auto const& a, auto const& b) noexcept {
			return a.position < b.position;
		});
	}
}

void Chart::calculate_object_timestamps() noexcept
{
	auto const beat_duration = duration_cast<nanoseconds>(duration<double>{60.0 / bpm});
	auto const measure_duration = beat_duration * 4;
	for (auto& lane: lanes) {
		for (auto& note: lane.notes) {
			note.timestamp = calculate_timestamp(note.position, measure_duration);
			note.length_ns = calculate_timestamp(note.length, measure_duration);
		}
	}
}

}
