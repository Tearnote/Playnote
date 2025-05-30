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
	static auto from_ir(IR const&) -> pair<Chart, io::BulkRequest>;

private:
	struct Note {
		nanoseconds timestamp;
		usize slot;
	};
	struct Lane {
		vector<Note> notes;
		usize next_note;
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
		static constexpr auto Stopped = -1zu;
		io::AudioCodec::Output audio;
		usize playback_pos;
	};

	Chart() = default; // Can only be created with factory methods

	string title;
	string artist;
	float bpm = 130.0f; // BMS spec default

	array<Lane, static_cast<usize>(LaneType::Size)> lanes;
	vector<optional<Slot>> slots;
};

auto Chart::from_ir(IR const& ir) -> pair<Chart, io::BulkRequest>
{
	auto result = pair<Chart, io::BulkRequest>{
		piecewise_construct,
		make_tuple(Chart{}),
		make_tuple(ir.get_path().parent_path())
	};
	auto& [chart, requests] = result;

	chart.slots.resize(ir.get_wav_slot_count());
	ir.each_header_event([&](IR::HeaderEvent const& event) {
		event.params.visit(visitor {
			[&](IR::HeaderEvent::Title* title_params) { chart.title = title_params->title; },
			[&](IR::HeaderEvent::Artist* artist_params) { chart.artist = artist_params->artist; },
			[&](IR::HeaderEvent::BPM* bpm_params) { chart.bpm = bpm_params->bpm; },
			[&](IR::HeaderEvent::WAV* wav_params) {
				chart.slots[wav_params->slot].emplace(Slot{});
				requests.enqueue<io::AudioCodec>(
					chart.slots[wav_params->slot]->audio,
					wav_params->name,
					{"wav", "ogg", "mp3", "flac", "opus"}, false
				);
			},
			[](auto&&) {}
		});
	});
	INFO("Built chart \"{}\"", chart.title);

	return result;
}

}
