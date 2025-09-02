/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/input.hpp:
A timestamped BMS chart input, and conversion to it via a predefined mapping.
*/

#pragma once
#include "preamble.hpp"
#include "config.hpp"
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
	Mapper();
	[[nodiscard]] auto from_key(threads::KeyInput const&, Playstyle) const -> optional<Input>;

private:
	array<array<threads::KeyInput::Code, +Chart::LaneType::Size>, +Playstyle::Size> key_bindings;
};

inline Mapper::Mapper()
{
	auto get_key = [](string_view conf) -> threads::KeyInput::Code {
		auto conf_entry = globals::config->get_entry<string>("controls", conf);
		return *enum_cast<threads::KeyInput::Code>(conf_entry).or_else([&]() -> optional<threads::KeyInput::Code> {
			throw runtime_error_fmt("Unknown keycode: {}", conf_entry);
		});
	};

	key_bindings[+Playstyle::_5K][+Chart::LaneType::P1_Key1] = get_key("5k_1");
	key_bindings[+Playstyle::_5K][+Chart::LaneType::P1_Key2] = get_key("5k_2");
	key_bindings[+Playstyle::_5K][+Chart::LaneType::P1_Key3] = get_key("5k_3");
	key_bindings[+Playstyle::_5K][+Chart::LaneType::P1_Key4] = get_key("5k_4");
	key_bindings[+Playstyle::_5K][+Chart::LaneType::P1_Key5] = get_key("5k_5");
	key_bindings[+Playstyle::_5K][+Chart::LaneType::P1_KeyS] = get_key("5k_s");

	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key1] = get_key("7k_1");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key2] = get_key("7k_2");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key3] = get_key("7k_3");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key4] = get_key("7k_4");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key5] = get_key("7k_5");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key6] = get_key("7k_6");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_Key7] = get_key("7k_7");
	key_bindings[+Playstyle::_7K][+Chart::LaneType::P1_KeyS] = get_key("7k_s");

	key_bindings[+Playstyle::_10K][+Chart::LaneType::P1_Key1] = get_key("10k_p1_1");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P1_Key2] = get_key("10k_p1_2");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P1_Key3] = get_key("10k_p1_3");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P1_Key4] = get_key("10k_p1_4");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P1_Key5] = get_key("10k_p1_5");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P1_KeyS] = get_key("10k_p1_s");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P2_Key1] = get_key("10k_p2_1");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P2_Key2] = get_key("10k_p2_2");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P2_Key3] = get_key("10k_p2_3");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P2_Key4] = get_key("10k_p2_4");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P2_Key5] = get_key("10k_p2_5");
	key_bindings[+Playstyle::_10K][+Chart::LaneType::P2_KeyS] = get_key("10k_p2_s");

	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key1] = get_key("14k_p1_1");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key2] = get_key("14k_p1_2");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key3] = get_key("14k_p1_3");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key4] = get_key("14k_p1_4");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key5] = get_key("14k_p1_5");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key6] = get_key("14k_p1_6");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_Key7] = get_key("14k_p1_7");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P1_KeyS] = get_key("14k_p1_s");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key1] = get_key("14k_p2_1");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key2] = get_key("14k_p2_2");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key3] = get_key("14k_p2_3");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key4] = get_key("14k_p2_4");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key5] = get_key("14k_p2_5");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key6] = get_key("14k_p2_6");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_Key7] = get_key("14k_p2_7");
	key_bindings[+Playstyle::_14K][+Chart::LaneType::P2_KeyS] = get_key("14k_p2_s");
}

inline auto Mapper::from_key(threads::KeyInput const& key, Playstyle playstyle) const -> optional<Input>
{
	auto const& playstyle_binds = key_bindings[+playstyle];
	auto match = find(playstyle_binds, key.code);
	if (match == playstyle_binds.end()) return nullopt;
	return Input{
		.timestamp = key.timestamp,
		.lane = static_cast<Chart::LaneType>(distance(playstyle_binds.begin(), match)),
		.state = key.state,
	};
}

}
