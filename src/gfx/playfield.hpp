/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "gfx/transform.hpp"
#include "gfx/renderer.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"
#include "bms/score.hpp"

namespace playnote::gfx {

class Playfield {
public:
	TransformRef transform;

	Playfield(Transform, float height, bms::Cursor const&, bms::Score const&);

	void enqueue(Renderer::Queue&, float scroll_speed, nanoseconds offset);

private:
	struct Field {
		float start;
		float length;
	};

	struct Note {
		enum Type {
			Odd,
			Even,
			Scratch,
			MeasureLine,
		};
		Type type;
		ssize_t lane_idx;
		TransformRef transform;
		float ln_height; // In transform units
	};

	float2 size;
	bms::Cursor const& cursor;
	bms::Score const& score;
	static_vector<Field, 2> fields;
	array<vector<Note>, enum_count<bms::Lane::Type>()> lanes;
	array<TransformRef, enum_count<bms::Lane::Type>()> lane_offsets;

	auto lane_order() const -> span<ssize_t const>;
	auto lane_to_note_type(bms::Lane::Type) const -> Note::Type;
	auto lane_background_color(bms::Lane::Type) const -> float4;
	auto lane_width(Note::Type) const -> float;
	auto note_color(Note::Type) const -> float4;
	auto note_size(Note::Type) const -> float2;
	auto judgement_color(bms::Score::JudgmentType) const -> float4;
	auto timing_color(bms::Score::Timing) const -> float4;
};

}
