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

// A position keeper that keeps track of the last movement's delta.
class Position {
public:
	vec2 position;
	vec2 velocity;

	Position() = default;
	explicit Position(vec2 pos): position{pos}, velocity{} {}
	explicit Position(float x, float y): position{x, y}, velocity{} {}

	void update(vec2 new_position);
};

inline void Position::update(vec2 new_position)
{
	velocity = new_position - position;
	position = new_position;
}

}
