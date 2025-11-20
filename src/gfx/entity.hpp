/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::gfx {

// A hierarchical position tracker that maintains last movement's delta.
class Position {
public:
	Position() = default;
	explicit Position(float2 pos): position{pos}, velocity{} {}
	explicit Position(float x, float y): position{x, y}, velocity{} {}

	void set_parent(Position& parent) { this->parent = parent; }
	void unset_parent() { parent = nullopt; }
	void move_to(float2 new_position);

	auto get_position() const -> float2;
	auto get_velocity() const -> float2;

private:
	float2 position;
	float2 velocity;
	optional<reference_wrapper<Position>> parent = nullopt;
};

inline void Position::move_to(float2 new_position)
{
	velocity = new_position - position;
	position = new_position;
}

inline auto Position::get_position() const -> float2
{
	if (parent)
		return position + parent->get().get_position();
	else
		return position;
}

inline auto Position::get_velocity() const -> float2
{
	if (parent)
		return velocity + parent->get().get_velocity();
	else
		return velocity;
}

}
