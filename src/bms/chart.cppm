/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart, constructed from an IR. Tracks playback and gameplay.
*/

module;
#include "macros/logger.hpp"

export module playnote.bms.chart;

import playnote.preamble;
import playnote.logger;
import playnote.io.bulk_request;
import playnote.io.audio_codec;
import playnote.bms.ir;

namespace playnote::bms {

export class Chart {
public:
	static auto from_ir(IR const&) noexcept -> Chart;

	[[nodiscard]] auto make_file_requests() noexcept -> io::BulkRequest;

private:
	static constexpr auto AudioExtensions = {"wav"sv, "ogg"sv, "mp3"sv, "flac"sv, "opus"sv};

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

	array<Lane, to_underlying(Lane::Type::Size)> lanes;

	struct WavSlot {
		static constexpr auto Stopped = -1zu;
		io::AudioCodec::Output audio;
		usize playback_pos;
	};

	vector<optional<WavSlot>> wav_slots;

	struct FileReferences {
		vector<optional<string>> wav;
	};

	FileReferences file_references;

	Chart() = default; // Can only be created with factory methods

	fs::path domain;
	string title;
	string artist;
	float bpm = 130.0f; // BMS spec default

	[[nodiscard]] static auto channel_to_lane(IR::ChannelEvent::Type) noexcept -> Lane::Type;

};

auto Chart::from_ir(IR const& ir) noexcept -> Chart
{
	auto chart = Chart{};

	chart.domain = ir.get_path().parent_path();
	chart.wav_slots.resize(ir.get_wav_slot_count());
	chart.file_references.wav.resize(ir.get_wav_slot_count());

	ir.each_header_event([&](IR::HeaderEvent const& event) noexcept {
		event.params.visit(visitor {
			[&](IR::HeaderEvent::Title* title_params) noexcept { chart.title = title_params->title; },
			[&](IR::HeaderEvent::Artist* artist_params) noexcept { chart.artist = artist_params->artist; },
			[&](IR::HeaderEvent::BPM* bpm_params) noexcept { chart.bpm = bpm_params->bpm; },
			[&](IR::HeaderEvent::WAV* wav_params) noexcept {
				chart.file_references.wav[wav_params->slot] = wav_params->name;
			},
			[](auto&&) noexcept {}
		});
	});

	ir.each_channel_event([&](IR::ChannelEvent const& event) noexcept {
		auto const lane_id = channel_to_lane(event.type);
		if (lane_id == Lane::Type::Size) return;
		chart.lanes[to_underlying(lane_id)].notes.emplace_back(Lane::Note{
			.position = event.position,
			.slot = event.slot,
		});
	});

	INFO("Built chart \"{}\"", chart.title);

	return chart;
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

auto Chart::channel_to_lane(IR::ChannelEvent::Type ch) noexcept -> Lane::Type
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
	case IR::ChannelEvent::Type::Note_P2_Key1: return Lane::Type::P2_Key1;
	case IR::ChannelEvent::Type::Note_P2_Key2: return Lane::Type::P2_Key2;
	case IR::ChannelEvent::Type::Note_P2_Key3: return Lane::Type::P2_Key3;
	case IR::ChannelEvent::Type::Note_P2_Key4: return Lane::Type::P2_Key4;
	case IR::ChannelEvent::Type::Note_P2_Key5: return Lane::Type::P2_Key5;
	case IR::ChannelEvent::Type::Note_P2_Key6: return Lane::Type::P2_Key6;
	case IR::ChannelEvent::Type::Note_P2_Key7: return Lane::Type::P2_Key7;
	default: return Lane::Type::Size;
	}
}

}
