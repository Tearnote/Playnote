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
	float2 position = {};

	Transform() = default;
	explicit Transform(float2 pos): position{pos} {}
	explicit Transform(float x, float y): position{x, y} {}

	auto set_parent(Transform& parent) -> Transform& { this->parent = parent; return *this; }
	auto unset_parent() -> Transform& { parent = nullopt; return *this; }

	// Run between frames to properly track velocity.
	auto update() -> Transform&;

	// Return position, taking into account parent transforms.
	auto global_position() const -> float2;

	// Return velocity, taking into account parent transforms.
	auto global_velocity() const -> float2;

private:
	float2 prev_position = position;
	optional<reference_wrapper<Transform>> parent = nullopt;
};

namespace globals {

inline auto transform_pool = Service<colony<Transform>>{};

using TransformRef = unique_ptr<Transform, decltype([](auto* t) {
	auto it = transform_pool->get_iterator(t);
	transform_pool->erase(it);
})>;

// Create a RAII-managed transform in the global pool. Guaranteed pointer stability.
template<typename... Args>
auto create_transform(Args&&... args) -> TransformRef
{ return TransformRef{&*transform_pool->emplace(forward<Args>(args)...)}; }

// Create a RAII-managed transform in the global pool that is parented to another transform
// already in the pool. Guaranteed pointer stability.
template<typename... Args>
auto create_child_transform(TransformRef const& parent, Args&&... args) -> TransformRef
{
	auto t = create_transform(forward<Args>(args)...);
	t->set_parent(*parent);
	return t;
}

// Update all transforms in the pool. Run once between frames.
inline void update_transforms()
{ for(auto& t: *transform_pool) t.update(); }

}

// A RAII handle to a transform on the global pool.
using TransformRef = globals::TransformRef;

inline auto Transform::update() -> Transform&
{
	prev_position = position;
	return *this;
}

inline auto Transform::global_position() const -> float2
{
	if (parent)
		return position + parent->get().global_position();
	else
		return position;
}

inline auto Transform::global_velocity() const -> float2
{
	if (parent)
		return position - prev_position + parent->get().global_velocity();
	else
		return position - prev_position;
}

}
