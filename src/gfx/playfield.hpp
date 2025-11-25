/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "bms/chart.hpp"
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "gfx/transform.hpp"
#include "gfx/renderer.hpp"
#include "bms/cursor.hpp"

namespace playnote::gfx {

class Playfield {
public:
	TransformRef transform;

	Playfield(Transform, float height, bms::Cursor const&);

	void enqueue(Renderer::Queue&, float scroll_speed);

private:
	enum class Side {
		Left,
		Right,
	};

	struct Note {
		enum Type {
			Odd,
			Even,
			Scratch,
			MeasureLine,
		};
		Type type;
		isize_t lane_idx;
		TransformRef transform;
		float ln_height; // In transform units
	};

	float height;
	bms::Cursor const& cursor;
	array<vector<Note>, enum_count<bms::Lane::Type>()> lanes;

	auto lane_to_note_type(bms::Lane::Type) const -> Note::Type;
	auto lane_offset(bms::Lane::Type) const -> float;
};

inline Playfield::Playfield(Transform transform, float height, bms::Cursor const& cursor):
	transform{globals::create_transform(transform)}, height{height}, cursor{cursor}
{}

inline void Playfield::enqueue(Renderer::Queue& queue, float scroll_speed)
{
	// Update notes

	// Remove judged notes
	for (auto [idx, lane]: lanes | views::enumerate) {
		auto const next_idx = cursor.next_note_idx(bms::Lane::Type{idx});
		auto range = remove_if(lane, [&](auto const& note) {
			return note.lane_idx < next_idx;
		});
		lane.erase(range.begin(), range.end());
	}

	// Add/modify remaining notes
	scroll_speed /= 4.0f; // 1 beat -> 1 standard measure
	scroll_speed *= 120.0f / cursor.get_chart().metadata.bpm_range.main; // Normalize to 120 BPM
	auto const max_distance = 1.0f / scroll_speed;
	cursor.upcoming_notes(max_distance, [&](bms::Note const& note, auto type, auto idx, auto distance) {
		auto& lane = lanes[+type];
		auto existing = find(lane, idx, &Note::lane_idx);
		if (existing == lane.end()) {
			lane.emplace_back(Note{
				.type = lane_to_note_type(type),
				.lane_idx = idx,
				.transform = globals::create_child_transform(transform,
					lane_offset(type),
					(1.0f - (distance / max_distance)) * height
				),
				.ln_height = note.type_is<bms::Note::LN>()?
					note.params<bms::Note::LN>().height / max_distance * height :
					0.0f,
			});
		} else {
			// ...
		}
	});

	// ...
}

inline auto Playfield::lane_to_note_type(bms::Lane::Type lane) const -> Note::Type
{
	if (cursor.get_chart().metadata.playstyle != bms::Playstyle::_9K) {
		switch (lane) {
			case bms::Lane::Type::P1_Key1:
			case bms::Lane::Type::P1_Key3:
			case bms::Lane::Type::P1_Key5:
			case bms::Lane::Type::P1_Key7:
			case bms::Lane::Type::P2_Key1:
			case bms::Lane::Type::P2_Key3:
			case bms::Lane::Type::P2_Key5:
			case bms::Lane::Type::P2_Key7:
				return Note::Type::Odd;
			case bms::Lane::Type::P1_Key2:
			case bms::Lane::Type::P1_Key4:
			case bms::Lane::Type::P1_Key6:
			case bms::Lane::Type::P2_Key2:
			case bms::Lane::Type::P2_Key4:
			case bms::Lane::Type::P2_Key6:
				return Note::Type::Even;
			case bms::Lane::Type::P1_KeyS:
			case bms::Lane::Type::P2_KeyS:
				return Note::Type::Scratch;
			case bms::Lane::Type::MeasureLine:
				return Note::Type::MeasureLine;
			default: PANIC();
		}
	} else {
		PANIC(); // PMS is unimplemented
	}
}

inline auto Playfield::lane_offset(bms::Lane::Type lane) const -> float
{
	//TODO
}

}
