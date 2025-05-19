/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/chart.cppm:
A definite, playable rhythm game chart, constructed from an IR. Tracks playback and gameplay.
*/

module;
#include <optional>
#include <vector>
#include <chrono>
#include <array>

export module playnote.bms.chart;

import playnote.stx.types;
import playnote.bms.ir;

namespace playnote::bms {

namespace chrono = std::chrono;
using stx::usize;

export class Chart {
public:
	static auto from_ir(IR const&) -> Chart;

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
		std::vector<float>& sample;
		int playback_pos;
	};

	Chart() = default;

	std::array<Lane, static_cast<usize>(LaneType::Size)> lanes;
	std::vector<std::optional<Slot>> slots;
};

}
