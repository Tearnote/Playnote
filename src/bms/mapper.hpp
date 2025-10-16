/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/mapper.hpp:
A timestamped BMS chart input, and conversion to it via a predefined mapping.
*/

#pragma once
#include "preamble.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"
#include "bms/chart.hpp"
#include "input.hpp"

namespace playnote::bms {

// A singular player input.
struct Input {
	nanoseconds timestamp; // Time since chart start
	Lane::Type lane;
	bool state; // true for pressed, false for released
};

class Mapper {
public:
	Mapper();
	[[nodiscard]] auto from_key(threads::KeyInput const&, Playstyle) -> optional<Input>;
	[[nodiscard]] auto from_button(threads::ButtonInput const&, Playstyle) -> optional<Input>;
	[[nodiscard]] auto submit_axis_input(threads::AxisInput const&, Playstyle) -> vector<Input>;
	[[nodiscard]] auto from_axis_state(Playstyle) -> vector<Input>;

private:
	struct ConBinding {
		threads::ControllerID controller;
		uint32 idx;
		auto operator==(ConBinding const&) const -> bool = default;
	};
	struct TurntableState {
		enum class Direction { CW, CCW, None };
		float value;
		float last_press_value;
		Direction direction;
		nanoseconds last_stopped;
	};

	array<array<threads::KeyInput::Code, enum_count<Lane::Type>()>, enum_count<Playstyle>()> key_bindings;
	array<array<optional<ConBinding>, enum_count<Lane::Type>()>, enum_count<Playstyle>()> button_bindings;
	array<array<optional<ConBinding>, 2>, enum_count<Playstyle>()> axis_bindings;
	array<array<TurntableState, 2>, enum_count<Playstyle>()> turntable_states;
	array<array<nanoseconds, enum_count<Lane::Type>()>, enum_count<Playstyle>()> last_input{};

	[[nodiscard]] static auto tt_difference(float prev, float curr) -> float;
	[[nodiscard]] static auto tt_direction(float prev, float curr) -> TurntableState::Direction;
};

inline Mapper::Mapper()
{
	auto get_key = [](string_view conf) -> threads::KeyInput::Code {
		auto conf_entry = globals::config->get_entry<string>("controls", conf);
		return *enum_cast<threads::KeyInput::Code>(conf_entry).or_else([&]() -> optional<threads::KeyInput::Code> {
			throw runtime_error_fmt("Unknown keycode: {}", conf_entry);
		});
	};

	auto get_con = [](string_view conf) -> optional<ConBinding> {
		auto conf_entry = globals::config->get_entry<string>("controls", conf);
		if (conf_entry == "None") return nullopt;
		auto segments = vector<string_view>{};
		copy(conf_entry | views::split(';') | views::to_sv, back_inserter(segments));
		if (segments.size() != 3)
			throw runtime_error_fmt("Invalid controller mapping syntax: {}", conf_entry);
		return ConBinding{
			.controller = { id{segments[0]}, lexical_cast<uint32>(segments[1]) },
			.idx = lexical_cast<uint32>(segments[2]),
		};
	};

	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key1] = get_key("kb_5k_1");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key2] = get_key("kb_5k_2");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key3] = get_key("kb_5k_3");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key4] = get_key("kb_5k_4");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key5] = get_key("kb_5k_5");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_KeyS] = get_key("kb_5k_s");

	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key1] = get_key("kb_7k_1");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key2] = get_key("kb_7k_2");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key3] = get_key("kb_7k_3");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key4] = get_key("kb_7k_4");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key5] = get_key("kb_7k_5");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key6] = get_key("kb_7k_6");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_Key7] = get_key("kb_7k_7");
	key_bindings[+Playstyle::_7K][+Lane::Type::P1_KeyS] = get_key("kb_7k_s");

	key_bindings[+Playstyle::_10K][+Lane::Type::P1_Key1] = get_key("kb_10k_p1_1");
	key_bindings[+Playstyle::_10K][+Lane::Type::P1_Key2] = get_key("kb_10k_p1_2");
	key_bindings[+Playstyle::_10K][+Lane::Type::P1_Key3] = get_key("kb_10k_p1_3");
	key_bindings[+Playstyle::_10K][+Lane::Type::P1_Key4] = get_key("kb_10k_p1_4");
	key_bindings[+Playstyle::_10K][+Lane::Type::P1_Key5] = get_key("kb_10k_p1_5");
	key_bindings[+Playstyle::_10K][+Lane::Type::P1_KeyS] = get_key("kb_10k_p1_s");
	key_bindings[+Playstyle::_10K][+Lane::Type::P2_Key1] = get_key("kb_10k_p2_1");
	key_bindings[+Playstyle::_10K][+Lane::Type::P2_Key2] = get_key("kb_10k_p2_2");
	key_bindings[+Playstyle::_10K][+Lane::Type::P2_Key3] = get_key("kb_10k_p2_3");
	key_bindings[+Playstyle::_10K][+Lane::Type::P2_Key4] = get_key("kb_10k_p2_4");
	key_bindings[+Playstyle::_10K][+Lane::Type::P2_Key5] = get_key("kb_10k_p2_5");
	key_bindings[+Playstyle::_10K][+Lane::Type::P2_KeyS] = get_key("kb_10k_p2_s");

	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key1] = get_key("kb_14k_p1_1");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key2] = get_key("kb_14k_p1_2");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key3] = get_key("kb_14k_p1_3");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key4] = get_key("kb_14k_p1_4");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key5] = get_key("kb_14k_p1_5");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key6] = get_key("kb_14k_p1_6");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_Key7] = get_key("kb_14k_p1_7");
	key_bindings[+Playstyle::_14K][+Lane::Type::P1_KeyS] = get_key("kb_14k_p1_s");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key1] = get_key("kb_14k_p2_1");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key2] = get_key("kb_14k_p2_2");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key3] = get_key("kb_14k_p2_3");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key4] = get_key("kb_14k_p2_4");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key5] = get_key("kb_14k_p2_5");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key6] = get_key("kb_14k_p2_6");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_Key7] = get_key("kb_14k_p2_7");
	key_bindings[+Playstyle::_14K][+Lane::Type::P2_KeyS] = get_key("kb_14k_p2_s");

	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key1] = get_key("kb_5k_1");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key2] = get_key("kb_5k_2");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key3] = get_key("kb_5k_3");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key4] = get_key("kb_5k_4");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_Key5] = get_key("kb_5k_5");
	key_bindings[+Playstyle::_5K][+Lane::Type::P1_KeyS] = get_key("kb_5k_s");

	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key1] = get_con("con_7k_1");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key2] = get_con("con_7k_2");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key3] = get_con("con_7k_3");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key4] = get_con("con_7k_4");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key5] = get_con("con_7k_5");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key6] = get_con("con_7k_6");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_Key7] = get_con("con_7k_7");
	button_bindings[+Playstyle::_7K][+Lane::Type::P1_KeyS] = get_con("con_7k_s");

	button_bindings[+Playstyle::_10K][+Lane::Type::P1_Key1] = get_con("con_10k_p1_1");
	button_bindings[+Playstyle::_10K][+Lane::Type::P1_Key2] = get_con("con_10k_p1_2");
	button_bindings[+Playstyle::_10K][+Lane::Type::P1_Key3] = get_con("con_10k_p1_3");
	button_bindings[+Playstyle::_10K][+Lane::Type::P1_Key4] = get_con("con_10k_p1_4");
	button_bindings[+Playstyle::_10K][+Lane::Type::P1_Key5] = get_con("con_10k_p1_5");
	button_bindings[+Playstyle::_10K][+Lane::Type::P1_KeyS] = get_con("con_10k_p1_s");
	button_bindings[+Playstyle::_10K][+Lane::Type::P2_Key1] = get_con("con_10k_p2_1");
	button_bindings[+Playstyle::_10K][+Lane::Type::P2_Key2] = get_con("con_10k_p2_2");
	button_bindings[+Playstyle::_10K][+Lane::Type::P2_Key3] = get_con("con_10k_p2_3");
	button_bindings[+Playstyle::_10K][+Lane::Type::P2_Key4] = get_con("con_10k_p2_4");
	button_bindings[+Playstyle::_10K][+Lane::Type::P2_Key5] = get_con("con_10k_p2_5");
	button_bindings[+Playstyle::_10K][+Lane::Type::P2_KeyS] = get_con("con_10k_p2_s");

	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key1] = get_con("con_14k_p1_1");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key2] = get_con("con_14k_p1_2");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key3] = get_con("con_14k_p1_3");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key4] = get_con("con_14k_p1_4");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key5] = get_con("con_14k_p1_5");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key6] = get_con("con_14k_p1_6");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_Key7] = get_con("con_14k_p1_7");
	button_bindings[+Playstyle::_14K][+Lane::Type::P1_KeyS] = get_con("con_14k_p1_s");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key1] = get_con("con_14k_p2_1");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key2] = get_con("con_14k_p2_2");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key3] = get_con("con_14k_p2_3");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key4] = get_con("con_14k_p2_4");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key5] = get_con("con_14k_p2_5");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key6] = get_con("con_14k_p2_6");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_Key7] = get_con("con_14k_p2_7");
	button_bindings[+Playstyle::_14K][+Lane::Type::P2_KeyS] = get_con("con_14k_p2_s");

	axis_bindings[+Playstyle::_5K][0] = get_con("con_5k_s_analog");
	axis_bindings[+Playstyle::_7K][0] = get_con("con_7k_s_analog");
	axis_bindings[+Playstyle::_10K][0] = get_con("con_10k_p1_s_analog");
	axis_bindings[+Playstyle::_10K][1] = get_con("con_10k_p2_s_analog");
	axis_bindings[+Playstyle::_14K][0] = get_con("con_14k_p1_s_analog");
	axis_bindings[+Playstyle::_14K][1] = get_con("con_14k_p2_s_analog");
}

inline auto Mapper::from_key(threads::KeyInput const& key, Playstyle playstyle) -> optional<Input>
{
	auto const& playstyle_binds = key_bindings[+playstyle];
	auto const match = find(playstyle_binds, key.code);
	if (match == playstyle_binds.end()) return nullopt;

	auto const lane = static_cast<Lane::Type>(distance(playstyle_binds.begin(), match));
	auto& last = last_input[+playstyle][+lane];
	auto const since_last = key.timestamp - last;
	if (since_last <= milliseconds{globals::config->get_entry<int32>("controls", "debounce_duration")}) return nullopt;

	last = key.timestamp;
	return Input{
		.timestamp = key.timestamp,
		.lane = lane,
		.state = key.state,
	};
}

inline auto Mapper::from_button(threads::ButtonInput const& button, Playstyle playstyle) -> optional<Input>
{
	auto const& playstyle_binds = button_bindings[+playstyle];
	auto input = ConBinding{button.controller, button.button};
	auto match = find_if(playstyle_binds, [&](auto const& bind) {
		if (!bind) return false;
		return *bind == input;
	});
	if (match == playstyle_binds.end()) return nullopt;

	auto const lane = static_cast<Lane::Type>(distance(playstyle_binds.begin(), match));
	auto& last = last_input[+playstyle][+lane];
	auto const since_last = button.timestamp - last;
	if (since_last <= milliseconds{globals::config->get_entry<int32>("controls", "debounce_duration")}) return nullopt;

	last = button.timestamp;
	return Input{
		.timestamp = button.timestamp,
		.lane = lane,
		.state = button.state,
	};
}

inline auto Mapper::submit_axis_input(threads::AxisInput const& axis, Playstyle playstyle) -> vector<Input>
{
	auto const& playstyle_binds = axis_bindings[+playstyle];
	auto input = ConBinding{axis.controller, axis.axis};
	auto match = find_if(playstyle_binds, [&](auto const& bind) {
		if (!bind) return false;
		return *bind == input;
	});
	if (match == playstyle_binds.end()) return {};
	auto tt_idx = distance(playstyle_binds.begin(), match);

	auto& tt_state = turntable_states[+playstyle][tt_idx];
	if (tt_state.value == axis.value) return {};

	auto inputs = vector<Input>{};
	auto lane = tt_idx == 0? Lane::Type::P1_KeyS : Lane::Type::P2_KeyS;
	auto current_direction = tt_direction(tt_state.value, axis.value);
	auto& last = last_input[+playstyle][+lane];
	auto const since_last = axis.timestamp - last;

	if (current_direction != tt_state.direction && since_last > milliseconds{globals::config->get_entry<int32>("controls", "debounce_duration")}) {
		// Changing direction of existing rotation
		if (tt_state.direction != TurntableState::Direction::None) {
			inputs.emplace_back(Input{
				.timestamp = axis.timestamp,
				.lane = lane,
				.state = false,
			});
		}

		// Starting new rotation
		inputs.emplace_back(Input{
			.timestamp = axis.timestamp,
			.lane = lane,
			.state = true,
		});
		tt_state.direction = current_direction;
		tt_state.last_press_value = axis.value;
		last = axis.timestamp;
	}
	tt_state.value = axis.value;
	tt_state.last_stopped = axis.timestamp;

	return inputs;
}

inline auto Mapper::from_axis_state(Playstyle playstyle) -> vector<Input>
{
	auto& turntables = turntable_states[+playstyle];
	auto inputs = vector<Input>{};

	// Handle delayed stopping
	for (auto [lane, tt]: views::zip(
		views::iota(0u) | views::transform([](auto idx) { return idx == 0? Lane::Type::P1_KeyS : Lane::Type::P2_KeyS; }),
		turntables)) {
		if (tt.direction == TurntableState::Direction::None) continue;
		auto now = globals::glfw->get_time();
		auto elapsed = now - tt.last_stopped;
		if (elapsed <= milliseconds{globals::config->get_entry<int32>("controls", "turntable_stop_timeout")}) continue;

		inputs.emplace_back(Input{
			.timestamp = now,
			.lane = lane,
			.state = false,
		});
		tt.direction = TurntableState::Direction::None;
	}

	return inputs;
}

inline auto Mapper::tt_difference(float prev, float curr) -> float
{
	auto diff = curr - prev;
	if (diff < -1.0f) diff += 2.0f;
	if (diff > 1.0f) diff -= 2.0f;
	return diff;
}

inline auto Mapper::tt_direction(float prev, float curr) -> TurntableState::Direction
{
	return tt_difference(prev, curr) > 0.0f? TurntableState::Direction::CW : TurntableState::Direction::CCW;
}

}
