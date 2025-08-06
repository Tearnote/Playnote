/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/playfield.cppm:
A renderable visual representation of a BMS chart playfield.
*/

module;
#include "gfx/renderer.hpp"
#include "preamble.hpp"
#include "assert.hpp"
#include "logger.hpp"

export module playnote.gfx.playfield;

import playnote.bms.cursor;
import playnote.bms.chart;

namespace playnote::gfx {

export class Playfield {
public:
	enum class Side {
		Left,
		Right,
	};

	Playfield(ivec2 position, int32 length, bms::Playstyle);

	void notes_from_cursor(bms::Cursor const&, float scroll_speed);

	void enqueue(Renderer::Queue&);

private:
	struct Note {
		// Unit used here: 0.0 is at the judgment line, 1.0 is at the top of the field
		float y_pos;
		float ln_height;
	};

	struct Lane {
		enum class Visual {
			Odd,
			Even,
			Scratch,
		};

		Visual visual;
		vector<Note> notes;
	};

	static constexpr auto LaneSeparatorWidth = 2;
	static constexpr auto LaneSeparatorColor = vec4{0.165f, 0.165f, 0.165f, 1.000f};
	static constexpr auto JudgmentLineHeight = 6;
	static constexpr auto JudgmentLineColor = vec4{1.000f, 0.200f, 0.200f, 1.000f};
	static constexpr auto FieldBorderWidth = 2;
	static constexpr auto FieldBorderColor = vec4{0.596f, 0.596f, 0.596f, 1.000f};
	static constexpr auto FieldSpacing = 94;
	static constexpr auto NoteHeight = 13;
	static constexpr auto MeasureLineHeight = 1;
	static constexpr auto MeasureLineColor = vec4{0.267f, 0.267f, 0.267f, 1.000f};

	ivec2 position;
	int32 length;
	bms::Playstyle playstyle;
	vector<vector<Lane>> fields;
	vector<float> measure_lines;

	[[nodiscard]] auto get_lane(bms::Chart::LaneType) -> Lane&;
	[[nodiscard]] static auto lane_width(Lane::Visual) -> int32;
	[[nodiscard]] static auto lane_background_color(Lane::Visual) -> vec4;
	[[nodiscard]] static auto lane_note_color(Lane::Visual) -> vec4;

	static auto make_field(bms::Playstyle, Side = Side::Left) -> vector<Lane>;
	static void enqueue_field_border(Renderer::Queue&, ivec2 position, ivec2 size);
	static void enqueue_lane(Renderer::Queue&, ivec2 position, int32 length, Lane const&, bool left_border);
	static void enqueue_measure_lines(Renderer::Queue&, span<float const> measure_lines, ivec2 position, ivec2 size);
};

Playfield::Playfield(ivec2 position, int32 length, bms::Playstyle playstyle):
	position{position},
	length{length},
	playstyle{playstyle}
{
	switch (playstyle) {
	case bms::Playstyle::_5K:
		fields.push_back(make_field(bms::Playstyle::_5K));
		break;
	case bms::Playstyle::_7K:
		fields.push_back(make_field(bms::Playstyle::_7K));
		break;
	case bms::Playstyle::_9K:
		fields.push_back(make_field(bms::Playstyle::_9K));
		break;
	case bms::Playstyle::_10K:
		fields.push_back(make_field(bms::Playstyle::_5K));
		fields.push_back(make_field(bms::Playstyle::_5K));
		break;
	case bms::Playstyle::_14K:
		fields.push_back(make_field(bms::Playstyle::_7K, Side::Left));
		fields.push_back(make_field(bms::Playstyle::_7K, Side::Right));
		break;
	}
}

void Playfield::notes_from_cursor(bms::Cursor const& cursor, float scroll_speed)
{
	for (auto& field: fields)
		for (auto& lane: field)
			lane.notes.clear();
	measure_lines.clear();

	scroll_speed /= 4.0f; // 1 beat -> 1 standard measure
	auto const max_distance = 1.0f / scroll_speed;
	cursor.upcoming_notes(max_distance, [&](bms::Note const& note, auto type, auto distance) {
		auto const y_pos = distance / max_distance;
		if (type == bms::Chart::LaneType::MeasureLine) {
			measure_lines.emplace_back(y_pos);
			return;
		}
		auto const ln_height = note.type_is<bms::Note::LN>()? note.params<bms::Note::LN>().height / max_distance : 0.0f;
		get_lane(type).notes.emplace_back(y_pos, ln_height);
	});
}

void Playfield::enqueue(Renderer::Queue& queue)
{
	auto x_advance = position.x();
	for (auto const& field: fields) {
		auto field_x_advance = 0;
		for (auto [idx, lane]: field | views::enumerate) {
			enqueue_lane(queue, {x_advance + field_x_advance, position.y()}, length, lane, idx != 0);
			field_x_advance += lane_width(lane.visual) + (idx != field.size() - 1? LaneSeparatorWidth : 0);
		}
		enqueue_measure_lines(queue, measure_lines, {x_advance, position.y()}, {field_x_advance, length});
		enqueue_field_border(queue, {x_advance, position.y()}, {field_x_advance, length});
		x_advance += field_x_advance + FieldSpacing;
	}
}

auto Playfield::get_lane(bms::Chart::LaneType type) -> Lane&
{
	switch (playstyle) {
	case bms::Playstyle::_5K:
	case bms::Playstyle::_7K:
	case bms::Playstyle::_14K:
		switch (type) {
		case bms::Chart::LaneType::P1_KeyS: return fields[0][0];
		case bms::Chart::LaneType::P1_Key1: return fields[0][1];
		case bms::Chart::LaneType::P1_Key2: return fields[0][2];
		case bms::Chart::LaneType::P1_Key3: return fields[0][3];
		case bms::Chart::LaneType::P1_Key4: return fields[0][4];
		case bms::Chart::LaneType::P1_Key5: return fields[0][5];
		case bms::Chart::LaneType::P1_Key6: return fields[0][6];
		case bms::Chart::LaneType::P1_Key7: return fields[0][7];
		case bms::Chart::LaneType::P2_Key1: return fields[1][0];
		case bms::Chart::LaneType::P2_Key2: return fields[1][1];
		case bms::Chart::LaneType::P2_Key3: return fields[1][2];
		case bms::Chart::LaneType::P2_Key4: return fields[1][3];
		case bms::Chart::LaneType::P2_Key5: return fields[1][4];
		case bms::Chart::LaneType::P2_Key6: return fields[1][5];
		case bms::Chart::LaneType::P2_Key7: return fields[1][6];
		case bms::Chart::LaneType::P2_KeyS: return fields[1][7];
		default: PANIC();
		}
	case bms::Playstyle::_10K:
		switch (type) {
		case bms::Chart::LaneType::P1_KeyS: return fields[0][0];
		case bms::Chart::LaneType::P1_Key1: return fields[0][1];
		case bms::Chart::LaneType::P1_Key2: return fields[0][2];
		case bms::Chart::LaneType::P1_Key3: return fields[0][3];
		case bms::Chart::LaneType::P1_Key4: return fields[0][4];
		case bms::Chart::LaneType::P1_Key5: return fields[0][5];
		case bms::Chart::LaneType::P2_KeyS: return fields[1][0];
		case bms::Chart::LaneType::P2_Key1: return fields[1][1];
		case bms::Chart::LaneType::P2_Key2: return fields[1][2];
		case bms::Chart::LaneType::P2_Key3: return fields[1][3];
		case bms::Chart::LaneType::P2_Key4: return fields[1][4];
		case bms::Chart::LaneType::P2_Key5: return fields[1][5];
		default: PANIC();
		}
	default: PANIC();
	}
}

auto Playfield::lane_width(Lane::Visual visual) -> int32
{
	switch (visual) {
	case Lane::Visual::Odd: return 40;
	case Lane::Visual::Even: return 32;
	case Lane::Visual::Scratch: return 72;
	default: PANIC();
	}
}

auto Playfield::lane_background_color(Lane::Visual visual) -> vec4
{
	switch (visual) {
	case Lane::Visual::Scratch:
	case Lane::Visual::Odd: return {0.000f, 0.000f, 0.000f, 1.000f};
	case Lane::Visual::Even: return {0.035f, 0.035f, 0.035f, 1.000f};
	default: PANIC();
	}
}

auto Playfield::lane_note_color(Lane::Visual visual) -> vec4
{
	switch (visual) {
	case Lane::Visual::Odd: return {0.800f, 0.800f, 0.800f, 1.000f};
	case Lane::Visual::Even: return {0.200f, 0.600f, 0.800f, 1.000f};
	case Lane::Visual::Scratch: return {0.800f, 0.200f, 0.200f, 1.000f};
	default: PANIC();
	}
}

auto Playfield::make_field(bms::Playstyle playstyle, Side side) -> vector<Lane>
{
	auto result = vector<Lane>{};
	switch (playstyle) {
	case bms::Playstyle::_5K:
		result.emplace_back(Lane::Visual::Scratch);
		result.emplace_back(Lane::Visual::Odd);
		result.emplace_back(Lane::Visual::Even);
		result.emplace_back(Lane::Visual::Odd);
		result.emplace_back(Lane::Visual::Even);
		result.emplace_back(Lane::Visual::Odd);
		return result;
	case bms::Playstyle::_7K:
		if (side == Side::Left) {
			result.emplace_back(Lane::Visual::Scratch);
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Even);
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Even);
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Even);
			result.emplace_back(Lane::Visual::Odd);
		} else {
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Even);
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Even);
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Even);
			result.emplace_back(Lane::Visual::Odd);
			result.emplace_back(Lane::Visual::Scratch);
		}
		return result;
	case bms::Playstyle::_9K: // TODO
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

void Playfield::enqueue_field_border(Renderer::Queue& queue, ivec2 position, ivec2 size)
{
	queue.enqueue_rect("judgment_line"_id, {
		{position.x(), position.y() + size.y() - JudgmentLineHeight},
		{size.x(), JudgmentLineHeight},
		JudgmentLineColor
	});
	queue.enqueue_rect("frame"_id, {
		{position.x() - FieldBorderWidth, position.y()},
		{FieldBorderWidth, size.y() + FieldBorderWidth},
		FieldBorderColor
	});
	queue.enqueue_rect("frame"_id, {
		{position.x() - FieldBorderWidth, size.y()},
		{size.x() + FieldBorderWidth * 2, FieldBorderWidth},
		FieldBorderColor
	});
	queue.enqueue_rect("frame"_id, {
		{position.x() + size.x(), position.y()},
		{FieldBorderWidth, size.y() + FieldBorderWidth},
		FieldBorderColor
	});
}

void Playfield::enqueue_lane(Renderer::Queue& queue, ivec2 position, int32 length, Lane const& lane, bool left_border)
{
	auto const width = lane_width(lane.visual);
	queue.enqueue_rect("frame"_id, {
		{position.x(), position.y()},
		{width, length},
		lane_background_color(lane.visual)});
	for (auto const& note: lane.notes) {
		auto const y_pos_clipped = max(0.0f, note.y_pos);
		auto const ln_overflow = max(0.0f, -note.y_pos);
		auto const ln_height_clipped = note.ln_height - ln_overflow;
		queue.enqueue_rect("notes"_id, {
			{position.x(), static_cast<int32>(position.y() + length - ceil((y_pos_clipped + ln_height_clipped) * length) - NoteHeight)},
			{width, NoteHeight + static_cast<int32>(ceil(ln_height_clipped * length))},
			lane_note_color(lane.visual)
		});
	}
	if (left_border) {
		queue.enqueue_rect("frame"_id, {
			{position.x() - LaneSeparatorWidth, position.y()},
			{LaneSeparatorWidth, length - JudgmentLineHeight},
			LaneSeparatorColor
		});
	}
}

void Playfield::enqueue_measure_lines(Renderer::Queue& queue, span<float const> measure_lines, ivec2 position, ivec2 size)
{
	for (auto y_pos: measure_lines) {
		queue.enqueue_rect("measure"_id, {
			{position.x(), static_cast<int32>(position.y() + size.y() - ceil(y_pos * size.y()) - MeasureLineHeight)},
			{size.x(), MeasureLineHeight},
			MeasureLineColor
		});
	}
}

}
