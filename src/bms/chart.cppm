/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart, constructed from an IR. Tracks playback and gameplay.
*/

module;
#include <optional>
#include <utility>
#include <vector>
#include <chrono>
#include <array>
#include "util/log_macros.hpp"

export module playnote.bms.chart;

import playnote.preamble;
import playnote.stx.callable;
import playnote.io.bulk_request;
import playnote.io.audio_codec;
import playnote.util.charset;
import playnote.bms.ir;
import playnote.globals;

namespace playnote::bms {

namespace chrono = std::chrono;
using io::BulkRequest;
using io::AudioCodec;
using util::UString;
using util::to_utf8;

export class Chart {
public:
	static auto from_ir(IR const&) -> std::pair<Chart, BulkRequest>;

private:
	struct Note {
		chrono::nanoseconds timestamp;
		int slot;
	};
	struct Lane {
		std::vector<Note> notes;
		int next_note;
	};
	enum class LaneType {
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
	struct Slot {
		AudioCodec::Output sample;
		int playback_pos;
	};

	Chart() = default;

	UString title;
	UString artist;
	float bpm{130}; // BMS spec default

	std::array<Lane, static_cast<usize>(LaneType::Size)> lanes;
	std::vector<std::optional<Slot>> slots;
};

auto Chart::from_ir(IR const& ir) -> std::pair<Chart, BulkRequest>
{
	auto chart = Chart{};

	auto path = ir.get_path();

	auto requests = BulkRequest{""/* todo */};

	chart.slots.resize(ir.get_wav_slot_count());
	ir.each_header_event([&](IR::HeaderEvent const& event) {
		event.params.visit(stx::visitor {
			[&](IR::HeaderEvent::Title const* title_params) { chart.title = title_params->title; },
			[&](IR::HeaderEvent::Artist const* artist_params) { chart.artist = artist_params->artist; },
			[&](IR::HeaderEvent::BPM const* bpm_params) { chart.bpm = bpm_params->bpm; },
			[&](IR::HeaderEvent::WAV const* wav_params) {
				chart.slots[wav_params->slot].emplace(Slot{});
				requests.enqueue<AudioCodec>(
					chart.slots[wav_params->slot]->sample,
					to_utf8(wav_params->name),
					{"wav", "ogg", "mp3", "flac", "opus"}
				);
			},
			[](auto&&) {}
		});
	});
	L_INFO("Built chart \"{}\"", to_utf8(chart.title));

	return std::make_pair(chart, requests);
}

}
