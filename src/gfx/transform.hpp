/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms. teehee~
*/

#pragma once
#include "preamble.hpp"
#include "utils/service.hpp"

namespace playnote::gfx {

// A stateful hierarchical transform.
class Transform {
public:
	Transform() = default;
	explicit Transform(float2 pos): position{pos}, velocity{} {}
	explicit Transform(float x, float y): position{x, y}, velocity{} {}

	void set_parent(Transform& parent) { this->parent = parent; }
	void unset_parent() { parent = nullopt; }
	void move_to(float2 new_position);

	auto get_position() const -> float2;
	auto get_velocity() const -> float2;

private:
	float2 position;
	float2 velocity;
	optional<reference_wrapper<Transform>> parent = nullopt;
};

inline void Transform::move_to(float2 new_position)
{
	velocity = new_position - position;
	position = new_position;
}

inline auto Transform::get_position() const -> float2
{
	if (parent)
		return position + parent->get().get_position();
	else
		return position;
}

inline auto Transform::get_velocity() const -> float2
{
	if (parent)
		return velocity + parent->get().get_velocity();
	else
		return velocity;
}

namespace globals {

inline auto transform_pool = Service<colony<Transform>>{};

using TransformRef = unique_ptr<Transform, decltype([](auto* t) {
	auto it = transform_pool->get_iterator(t);
	transform_pool->erase(it);
})>;

template<typename... Args>
auto create_transform(Args&&... args) -> TransformRef
{
	return TransformRef{&*transform_pool->emplace(forward<Args>(args)...)};
}

}

using TransformRef = globals::TransformRef;

}
