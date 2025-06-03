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
	struct Lane {
		struct Note {
			using Position = NotePosition;
			Position position;
			nanoseconds timestamp;
			usize slot;
		};
		enum class Type: usize {
			BGM,
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
			Size,
		};

		vector<Note> notes;
		usize next_note;
	};

public:
	using Note = Lane::Note;
	using NoteType = Lane::Type;

	static auto from_ir(IR const&) noexcept -> Chart;

	void restart() noexcept;

	[[nodiscard]] auto make_file_requests() noexcept -> io::BulkRequest;

	template<callable<void(lib::pw::Sample)> Func>
	void advance_one_sample(Func&& func) noexcept;

	template<callable<void(Note const&, NoteType, nanoseconds)> Func>
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

	[[nodiscard]] static auto note_channel_to_lane(IR::ChannelEvent::Type) noexcept -> Lane::Type;

	[[nodiscard]] static auto progress_to_ns(usize) noexcept -> nanoseconds;

	void process_ir_headers(IR const&) noexcept;
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

template<callable<void(Chart::Note const&, Chart::NoteType, nanoseconds)> Func>
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
			func(note, static_cast<NoteType>(&lane - &lanes.front()), distance);
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

	ir.each_channel_event([&](IR::ChannelEvent const& event) noexcept {
		auto const lane_id = note_channel_to_lane(event.type);
		if (lane_id == Lane::Type::Size) return;
		chart.lanes[to_underlying(lane_id)].notes.emplace_back(Lane::Note{
			.position = event.position,
			.slot = event.slot,
		});
	});

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

auto Chart::progress_to_ns(usize samples) noexcept -> nanoseconds
{
	auto const sampling_rate = io::AudioCodec::sampling_rate;
	ASSERT(sampling_rate > 0);
	auto const ns_per_sample = duration_cast<nanoseconds>(duration<double>{1.0 / sampling_rate});
	auto const whole_seconds = samples / sampling_rate;
	auto const remainder = samples % sampling_rate;
	return 1s * whole_seconds + ns_per_sample * remainder;
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
			auto const measures = note.position.numerator() / note.position.denominator();
			note.timestamp = measures * measure_duration;
			auto const proper_numerator = note.position.numerator() % note.position.denominator();
			note.timestamp += (measure_duration / note.position.denominator()) * proper_numerator;
		}
	}
}

}
