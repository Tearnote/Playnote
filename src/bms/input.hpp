/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/input.hpp:
A timestamped BMS chart input, and conversion to it via a predefined mapping.
*/

#pragma once
#include "preamble.hpp"
#include "bms/chart.hpp"
#include "threads/input_shouts.hpp"

namespace playnote::bms {

// A singular player input.
struct Input {
	nanoseconds timestamp; // Time since chart start
	Chart::LaneType lane;
	bool state; // true for pressed, false for released
};

class Mapper {
public:
	[[nodiscard]] auto from_key(threads::KeyInput const&) -> optional<Input>;
};

inline auto Mapper::from_key(threads::KeyInput const& key) -> optional<Input>
{
	auto result = Input{
		.timestamp = key.timestamp,
		.state = key.state,
	};
	switch (key.code) {
	case threads::KeyInput::Code::Q: result.lane = Chart::LaneType::P1_Key1; break;
	case threads::KeyInput::Code::Two: result.lane = Chart::LaneType::P1_Key2; break;
	case threads::KeyInput::Code::W: result.lane = Chart::LaneType::P1_Key3; break;
	case threads::KeyInput::Code::Three: result.lane = Chart::LaneType::P1_Key4; break;
	case threads::KeyInput::Code::E: result.lane = Chart::LaneType::P1_Key5; break;
	case threads::KeyInput::Code::Four: result.lane = Chart::LaneType::P1_Key6; break;
	case threads::KeyInput::Code::R: result.lane = Chart::LaneType::P1_Key7; break;
	case threads::KeyInput::Code::Tab:
	case threads::KeyInput::Code::One: result.lane = Chart::LaneType::P1_KeyS; break;
	case threads::KeyInput::Code::O: result.lane = Chart::LaneType::P2_Key1; break;
	case threads::KeyInput::Code::Zero: result.lane = Chart::LaneType::P2_Key2; break;
	case threads::KeyInput::Code::P: result.lane = Chart::LaneType::P2_Key3; break;
	case threads::KeyInput::Code::Minus: result.lane = Chart::LaneType::P2_Key4; break;
	case threads::KeyInput::Code::LeftBracket: result.lane = Chart::LaneType::P2_Key5; break;
	case threads::KeyInput::Code::Equal: result.lane = Chart::LaneType::P2_Key6; break;
	case threads::KeyInput::Code::RightBracket: result.lane = Chart::LaneType::P2_Key7; break;
	case threads::KeyInput::Code::Backslash:
	case threads::KeyInput::Code::Backspace: result.lane = Chart::LaneType::P2_KeyS; break;
	default: return nullopt;
	}
	return result;
}

}
