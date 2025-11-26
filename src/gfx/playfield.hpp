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
	static constexpr auto FieldSpacing = 94.0f;

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
	array<TransformRef, enum_count<bms::Lane::Type>()> lane_offsets;

	auto lane_to_note_type(bms::Lane::Type) const -> Note::Type;
	auto note_width(Note::Type) const -> float;
	auto lane_order() const -> span<isize_t const>;
};

inline Playfield::Playfield(Transform transform, float height, bms::Cursor const& cursor):
	transform{globals::create_transform(transform)}, height{height}, cursor{cursor}
{
	// Precalc lane offsets
	auto order = lane_order();
	auto offset = 0.0f;
	for (auto lane_idx: order) {
		if (lane_idx == -1) {
			offset += FieldSpacing;
			continue;
		}
		auto const lane_type = bms::Lane::Type{lane_idx};
		lane_offsets[lane_idx] = globals::create_child_transform(this->transform, offset, 0.0f);
		auto const note_type = lane_to_note_type(lane_type);
		auto const width = note_width(note_type);
		offset += width;
	}
	lane_offsets[+bms::Lane::Type::MeasureLine] = globals::create_child_transform(this->transform);
}

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
				.transform = globals::create_child_transform(lane_offsets[+type],
					0.0f, (1.0f - (distance / max_distance)) * height),
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

inline auto Playfield::note_width(Note::Type type) const -> float
{
	switch (type) {
		case Note::Type::Odd:
			return 40.0f;
		case Note::Type::Even:
			return 32.0f;
		case Note::Type::Scratch:
			return 72.0f;
		default: PANIC();
	}
}

inline auto Playfield::lane_order() const -> span<isize_t const>
{
	static constexpr auto LaneOrder5K = to_array<isize_t>({
		+bms::Lane::Type::P1_KeyS,
		+bms::Lane::Type::P1_Key1,
		+bms::Lane::Type::P1_Key2,
		+bms::Lane::Type::P1_Key3,
		+bms::Lane::Type::P1_Key4,
		+bms::Lane::Type::P1_Key5,
	});
	static constexpr auto LaneOrder7K = to_array<isize_t>({
		+bms::Lane::Type::P1_KeyS,
		+bms::Lane::Type::P1_Key1,
		+bms::Lane::Type::P1_Key2,
		+bms::Lane::Type::P1_Key3,
		+bms::Lane::Type::P1_Key4,
		+bms::Lane::Type::P1_Key5,
		+bms::Lane::Type::P1_Key6,
		+bms::Lane::Type::P1_Key7,
	});
	static constexpr auto LaneOrder10K = to_array<isize_t>({
		+bms::Lane::Type::P1_KeyS,
		+bms::Lane::Type::P1_Key1,
		+bms::Lane::Type::P1_Key2,
		+bms::Lane::Type::P1_Key3,
		+bms::Lane::Type::P1_Key4,
		+bms::Lane::Type::P1_Key5,
		-1,
		+bms::Lane::Type::P2_KeyS,
		+bms::Lane::Type::P2_Key1,
		+bms::Lane::Type::P2_Key2,
		+bms::Lane::Type::P2_Key3,
		+bms::Lane::Type::P2_Key4,
		+bms::Lane::Type::P2_Key5,
	});
	static constexpr auto LaneOrder14K = to_array<isize_t>({
		+bms::Lane::Type::P1_KeyS,
		+bms::Lane::Type::P1_Key1,
		+bms::Lane::Type::P1_Key2,
		+bms::Lane::Type::P1_Key3,
		+bms::Lane::Type::P1_Key4,
		+bms::Lane::Type::P1_Key5,
		+bms::Lane::Type::P1_Key6,
		+bms::Lane::Type::P1_Key7,
		-1,
		+bms::Lane::Type::P2_Key1,
		+bms::Lane::Type::P2_Key2,
		+bms::Lane::Type::P2_Key3,
		+bms::Lane::Type::P2_Key4,
		+bms::Lane::Type::P2_Key5,
		+bms::Lane::Type::P2_Key6,
		+bms::Lane::Type::P2_Key7,
		+bms::Lane::Type::P2_KeyS,
	});
	switch (cursor.get_chart().metadata.playstyle) {
	case bms::Playstyle::_5K: return LaneOrder5K;
	case bms::Playstyle::_7K: return LaneOrder7K;
	case bms::Playstyle::_10K: return LaneOrder10K;
	case bms::Playstyle::_14K: return LaneOrder14K;
	default: PANIC();
	}
}

}
