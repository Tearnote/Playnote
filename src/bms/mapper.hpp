/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
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
	[[nodiscard]] auto from_key(KeyInput const&, Playstyle) -> optional<Input>;
	[[nodiscard]] auto from_button(ButtonInput const&, Playstyle) -> optional<Input>;
	[[nodiscard]] auto submit_axis_input(AxisInput const&, Playstyle) -> static_vector<Input, 2>;
	[[nodiscard]] auto from_axis_state(Playstyle) -> static_vector<Input, 2>;

private:
	struct ConBinding {
		ControllerID controller;
		int idx;
		auto operator==(ConBinding const&) const -> bool = default;
	};
	struct TurntableState {
		enum class Direction { CW, CCW, None };
		float value;
		float last_press_value;
		Direction direction;
		nanoseconds last_stopped;
	};

	array<array<KeyInput::Code, enum_count<Lane::Type>()>, enum_count<Playstyle>()> key_bindings;
	array<array<optional<ConBinding>, enum_count<Lane::Type>()>, enum_count<Playstyle>()> button_bindings;
	array<array<optional<ConBinding>, 2>, enum_count<Playstyle>()> axis_bindings;
	array<array<TurntableState, 2>, enum_count<Playstyle>()> turntable_states;
	array<array<nanoseconds, enum_count<Lane::Type>()>, enum_count<Playstyle>()> last_input{};

	[[nodiscard]] static auto tt_difference(float prev, float curr) -> float;
	[[nodiscard]] static auto tt_direction(float prev, float curr) -> TurntableState::Direction;
};

}
