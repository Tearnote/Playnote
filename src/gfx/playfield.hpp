/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "gfx/transform.hpp"
#include "gfx/renderer.hpp"
#include "bms/cursor.hpp"

namespace playnote::gfx {

class Playfield {
public:
	Transform transform;

	Playfield(Transform, float height, bms::Cursor const&);

	void enqueue(Renderer::Queue&, float scroll_speed);

private:
	enum class Side {
		Left,
		Right,
	};

	struct Note {
		isize_t idx;
		Transform transform;
		float ln_height; // Fraction of height
	};

	struct Lane {
		enum class Visual {
			Odd,
			Even,
			Scratch,
		};

		Transform transform;
		Visual visual;
		bms::Lane::Type type;
		vector<Note> notes;
	};

	float height;
	bms::Cursor const& cursor;
	vector<vector<Lane>> fields;
	vector<Transform> measure_lines;

	static auto make_field(bms::Playstyle, Side = Side::Left) -> vector<Lane>;
};

inline Playfield::Playfield(Transform transform, float height, bms::Cursor const& cursor):
	transform{transform}, height{height}, cursor{cursor}
{
	switch (cursor.get_chart().metadata.playstyle) {
	case bms::Playstyle::_5K:
		fields.emplace_back(make_field(bms::Playstyle::_5K));
		break;
	case bms::Playstyle::_7K:
		fields.emplace_back(make_field(bms::Playstyle::_7K));
		break;
	case bms::Playstyle::_9K:
		fields.emplace_back(make_field(bms::Playstyle::_9K));
		break;
	case bms::Playstyle::_10K:
		fields.emplace_back(make_field(bms::Playstyle::_5K));
		fields.emplace_back(make_field(bms::Playstyle::_5K));
		break;
	case bms::Playstyle::_14K:
		fields.emplace_back(make_field(bms::Playstyle::_7K, Side::Left));
		fields.emplace_back(make_field(bms::Playstyle::_7K, Side::Right));
		break;
	}
}

inline void Playfield::enqueue(Renderer::Queue& queue, float scroll_speed)
{
	// Update notes

	// Remove judged notes
	for (auto& field: fields) {
		for (auto& lane: field) {
			auto const next_note_idx = cursor.next_note_idx(lane.type);
			auto range = remove_if(lane.notes, [&](Note const& note) {
				return note.idx < next_note_idx;
			});
			lane.notes.erase(range.begin(), range.end());
		}
	}

	// Add/modify remaining notes
	scroll_speed /= 4.0f; // 1 beat -> 1 standard measure
	scroll_speed *= 120.0f / cursor.get_chart().metadata.bpm_range.main; // Normalize to 120 BPM
	auto const max_distance = 1.0f / scroll_speed;
	cursor.upcoming_notes(max_distance, [&](bms::Note const& note, auto type, auto idx, auto distance) {
		auto& lane = [&] -> Lane& {
			for (auto& field: fields)
				for (auto& lane: field)
					if (lane.type == type) return lane;
			PANIC();
		}();
		auto existing = find(lane.notes, idx, &Note::idx);
	});
}

inline auto Playfield::make_field(bms::Playstyle playstyle, Side side) -> vector<Lane>
{
	auto result = vector<Lane>{};
	switch (playstyle) {
	case bms::Playstyle::_5K:
		result.emplace_back(Lane::Visual::Scratch, bms::Lane::Type::P1_KeyS);
		result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key1);
		result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P1_Key2);
		result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key3);
		result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P1_Key4);
		result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key5);
		return result;
	case bms::Playstyle::_7K:
		if (side == Side::Left) {
			result.emplace_back(Lane::Visual::Scratch, bms::Lane::Type::P1_KeyS);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key1);
			result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P1_Key2);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key3);
			result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P1_Key4);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key5);
			result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P1_Key6);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P1_Key7);
		} else {
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P2_Key1);
			result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P2_Key2);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P2_Key3);
			result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P2_Key4);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P2_Key5);
			result.emplace_back(Lane::Visual::Even,    bms::Lane::Type::P2_Key6);
			result.emplace_back(Lane::Visual::Odd,     bms::Lane::Type::P2_Key7);
			result.emplace_back(Lane::Visual::Scratch, bms::Lane::Type::P2_KeyS);
		}
		return result;
	case bms::Playstyle::_9K: //TODO
		result.emplace_back(Lane::Visual::Odd);
		result.emplace_back(Lane::Visual::Even);
		result.emplace_back(Lane::Visual::Odd);
		result.emplace_back(Lane::Visual::Even);
		result.emplace_back(Lane::Visual::Odd);
		result.emplace_back(Lane::Visual::Even);
		result.emplace_back(Lane::Visual::Odd);
		result.emplace_back(Lane::Visual::Even);
		result.emplace_back(Lane::Visual::Odd);
		return result;
	default: PANIC();
	}
}

}
