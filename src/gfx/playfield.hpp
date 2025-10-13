/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

gfx/playfield.hpp:
A renderable visual representation of a BMS chart playfield.
*/

#pragma once
#include "preamble.hpp"
#include "assert.hpp"
#include "config.hpp"
#include "lib/imgui.hpp"
#include "gfx/renderer.hpp"
#include "bms/cursor.hpp"
#include "bms/score.hpp"
#include "bms/chart.hpp"

namespace playnote::gfx {

class Playfield {
public:
	enum class Side {
		Left,
		Right,
	};

	Playfield(ivec2 position, int32 length, bms::Playstyle);

	void enqueue_from_cursor(Renderer::Queue&, bms::Cursor const&, bms::Score const&, float scroll_speed, nanoseconds offset);

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
		bms::Lane::Type type;
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
	static constexpr auto LanePressedMargin = 4;
	static constexpr auto LanePressedColor = vec4{1.000f, 1.000f, 1.000f, 1.000f};

	ivec2 position;
	int32 length;
	bms::Playstyle playstyle;
	vector<vector<Lane>> fields;
	vector<float> measure_lines;

	[[nodiscard]] auto get_lane(bms::Lane::Type) -> Lane&;
	[[nodiscard]] static auto lane_width(Lane::Visual) -> int32;
	[[nodiscard]] static auto lane_background_color(Lane::Visual) -> vec4;
	[[nodiscard]] static auto lane_note_color(Lane::Visual) -> vec4;
	[[nodiscard]] static auto judgement_color(bms::Score::JudgmentType) -> vec4;
	[[nodiscard]] static auto timing_color(bms::Score::Timing) -> vec4;

	static auto make_field(bms::Playstyle, Side = Side::Left) -> vector<Lane>;
	static void enqueue_field_border(Renderer::Queue&, ivec2 position, ivec2 size);
	static void enqueue_lane(Renderer::Queue&, ivec2 position, int32 length, Lane const&, bool left_border, bool pressed);
	static void enqueue_measure_lines(Renderer::Queue&, span<float const> measure_lines, ivec2 position, ivec2 size);
	static void enqueue_judgment(bms::Score::Judgment const&, uint32 field_id, ivec2 position, ivec2 size);
};

inline Playfield::Playfield(ivec2 position, int32 length, bms::Playstyle playstyle):
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

inline void Playfield::enqueue_from_cursor(Renderer::Queue& queue, bms::Cursor const& cursor, bms::Score const& score,
	float scroll_speed, nanoseconds offset)
{
	for (auto& field: fields)
		for (auto& lane: field)
			lane.notes.clear();
	measure_lines.clear();

	// Retrieve visible notes
	scroll_speed /= 4.0f; // 1 beat -> 1 standard measure
	scroll_speed *= 120.0f / cursor.get_chart().metadata.bpm_range.main; // Normalize to 120 BPM
	auto const max_distance = 1.0f / scroll_speed;
	cursor.upcoming_notes(max_distance, [&](bms::Note const& note, auto type, auto distance) {
		auto const y_pos = distance / max_distance;
		if (type == bms::Lane::Type::MeasureLine) {
			measure_lines.emplace_back(y_pos);
			return;
		}
		auto const ln_height = note.type_is<bms::Note::LN>()? note.params<bms::Note::LN>().height / max_distance : 0.0f;
		get_lane(type).notes.emplace_back(y_pos, ln_height);
	}, offset, true);

	// Enqueue graphics
	auto x_advance = position.x();
	for (auto [idx, field]: fields | views::enumerate) {
		auto field_x_advance = 0;
		for (auto [l_idx, lane]: field | views::enumerate) {
			enqueue_lane(queue, {x_advance + field_x_advance, position.y()}, length, lane,
				l_idx != 0, cursor.is_pressed(lane.type));
			field_x_advance += lane_width(lane.visual) + (l_idx != field.size() - 1? LaneSeparatorWidth : 0);
		}
		enqueue_measure_lines(queue, measure_lines, {x_advance, position.y()}, {field_x_advance, length});
		enqueue_field_border(queue, {x_advance, position.y()}, {field_x_advance, length});
		auto const judgment = score.get_latest_judgment(idx);
		if (judgment && cursor.get_progress_ns() - judgment->timestamp <= milliseconds{globals::config->get_entry<int>("gameplay", "judgment_timeout")})
			enqueue_judgment(*judgment, idx, {x_advance, position.y()}, {field_x_advance, length});
		x_advance += field_x_advance + FieldSpacing;
	}
}

inline auto Playfield::get_lane(bms::Lane::Type type) -> Lane&
{
	switch (playstyle) {
	case bms::Playstyle::_5K:
	case bms::Playstyle::_7K:
	case bms::Playstyle::_14K:
		switch (type) {
		case bms::Lane::Type::P1_KeyS: return fields[0][0];
		case bms::Lane::Type::P1_Key1: return fields[0][1];
		case bms::Lane::Type::P1_Key2: return fields[0][2];
		case bms::Lane::Type::P1_Key3: return fields[0][3];
		case bms::Lane::Type::P1_Key4: return fields[0][4];
		case bms::Lane::Type::P1_Key5: return fields[0][5];
		case bms::Lane::Type::P1_Key6: return fields[0][6];
		case bms::Lane::Type::P1_Key7: return fields[0][7];
		case bms::Lane::Type::P2_Key1: return fields[1][0];
		case bms::Lane::Type::P2_Key2: return fields[1][1];
		case bms::Lane::Type::P2_Key3: return fields[1][2];
		case bms::Lane::Type::P2_Key4: return fields[1][3];
		case bms::Lane::Type::P2_Key5: return fields[1][4];
		case bms::Lane::Type::P2_Key6: return fields[1][5];
		case bms::Lane::Type::P2_Key7: return fields[1][6];
		case bms::Lane::Type::P2_KeyS: return fields[1][7];
		default: PANIC();
		}
	case bms::Playstyle::_10K:
		switch (type) {
		case bms::Lane::Type::P1_KeyS: return fields[0][0];
		case bms::Lane::Type::P1_Key1: return fields[0][1];
		case bms::Lane::Type::P1_Key2: return fields[0][2];
		case bms::Lane::Type::P1_Key3: return fields[0][3];
		case bms::Lane::Type::P1_Key4: return fields[0][4];
		case bms::Lane::Type::P1_Key5: return fields[0][5];
		case bms::Lane::Type::P2_KeyS: return fields[1][0];
		case bms::Lane::Type::P2_Key1: return fields[1][1];
		case bms::Lane::Type::P2_Key2: return fields[1][2];
		case bms::Lane::Type::P2_Key3: return fields[1][3];
		case bms::Lane::Type::P2_Key4: return fields[1][4];
		case bms::Lane::Type::P2_Key5: return fields[1][5];
		default: PANIC();
		}
	default: PANIC();
	}
}

inline auto Playfield::lane_width(Lane::Visual visual) -> int32
{
	switch (visual) {
	case Lane::Visual::Odd:     return 40;
	case Lane::Visual::Even:    return 32;
	case Lane::Visual::Scratch: return 72;
	default: PANIC();
	}
}

inline auto Playfield::lane_background_color(Lane::Visual visual) -> vec4
{
	switch (visual) {
	case Lane::Visual::Scratch:
	case Lane::Visual::Odd:  return {0.000f, 0.000f, 0.000f, 1.000f};
	case Lane::Visual::Even: return {0.035f, 0.035f, 0.035f, 1.000f};
	default: PANIC();
	}
}

inline auto Playfield::lane_note_color(Lane::Visual visual) -> vec4
{
	switch (visual) {
	case Lane::Visual::Odd:     return {0.800f, 0.800f, 0.800f, 1.000f};
	case Lane::Visual::Even:    return {0.200f, 0.600f, 0.800f, 1.000f};
	case Lane::Visual::Scratch: return {0.800f, 0.200f, 0.200f, 1.000f};
	default: PANIC();
	}
}

inline auto Playfield::judgement_color(bms::Score::JudgmentType judge) -> vec4
{
	switch (judge) {
	case bms::Score::JudgmentType::PGreat: return {0.533f, 0.859f, 0.961f, 1.000f};
	case bms::Score::JudgmentType::Great:  return {0.980f, 0.863f, 0.380f, 1.000f};
	case bms::Score::JudgmentType::Good:   return {0.796f, 0.576f, 0.191f, 1.000f};
	case bms::Score::JudgmentType::Bad:    return {0.933f, 0.525f, 0.373f, 1.000f};
	case bms::Score::JudgmentType::Poor:   return {0.606f, 0.207f, 0.171f, 1.000f};
	}
}

inline auto Playfield::timing_color(bms::Score::Timing timing) -> vec4
{
	switch (timing) {
	case bms::Score::Timing::Early: return {0.200f, 0.400f, 0.961f, 1.000f};
	case bms::Score::Timing::Late:  return {0.933f, 0.300f, 0.300f, 1.000f};
	default:                        return {1.000f, 1.000f, 1.000f, 1.000f};
	}
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

inline void Playfield::enqueue_field_border(Renderer::Queue& queue, ivec2 position, ivec2 size)
{
	queue.enqueue_rect("judgment_line"_id, {
		{position.x(), position.y() + size.y() - JudgmentLineHeight},
		{size.x(), JudgmentLineHeight},
		JudgmentLineColor,
	});
	queue.enqueue_rect("frame"_id, {
		{position.x() - FieldBorderWidth, position.y()},
		{FieldBorderWidth, size.y() + FieldBorderWidth},
		FieldBorderColor,
	});
	queue.enqueue_rect("frame"_id, {
		{position.x() - FieldBorderWidth, size.y()},
		{size.x() + FieldBorderWidth * 2, FieldBorderWidth},
		FieldBorderColor,
	});
	queue.enqueue_rect("frame"_id, {
		{position.x() + size.x(), position.y()},
		{FieldBorderWidth, size.y() + FieldBorderWidth},
		FieldBorderColor,
	});
}

inline void Playfield::enqueue_lane(Renderer::Queue& queue, ivec2 position, int32 length, Lane const& lane, bool left_border, bool pressed)
{
	auto const width = lane_width(lane.visual);
	queue.enqueue_rect("frame"_id, {
		{position.x(), position.y()},
		{width, length},
		lane_background_color(lane.visual)});
	for (auto const& note: lane.notes) {
		if (note.y_pos + note.ln_height < 0.0f) continue;
		auto const y_pos_clipped = max(0.0f, note.y_pos);
		auto const ln_overflow = max(0.0f, -note.y_pos);
		auto const ln_height_clipped = note.ln_height - ln_overflow;
		queue.enqueue_rect("notes"_id, {
			{position.x(), static_cast<int32>(position.y() + length - ceil((y_pos_clipped + ln_height_clipped) * length) - NoteHeight)},
			{width, NoteHeight + static_cast<int32>(ceil(ln_height_clipped * length))},
			lane_note_color(lane.visual),
		});
	}
	if (left_border) {
		queue.enqueue_rect("frame"_id, {
			{position.x() - LaneSeparatorWidth, position.y()},
			{LaneSeparatorWidth, length - JudgmentLineHeight},
			LaneSeparatorColor,
		});
	}
	if (pressed) {
		queue.enqueue_rect("pressed"_id, {
			{position.x() + LanePressedMargin, position.y() + length + LanePressedMargin * 2 + FieldBorderWidth},
			{width - LanePressedMargin * 2, width - LanePressedMargin * 2},
			LanePressedColor,
		});
	}
}

inline void Playfield::enqueue_measure_lines(Renderer::Queue& queue, span<float const> measure_lines, ivec2 position, ivec2 size)
{
	for (auto y_pos: measure_lines) {
		queue.enqueue_rect("measure"_id, {
			{position.x(), static_cast<int32>(position.y() + size.y() - ceil(y_pos * size.y()) - MeasureLineHeight)},
			{size.x(), MeasureLineHeight},
			MeasureLineColor,
		});
	}
}

inline void Playfield::enqueue_judgment(bms::Score::Judgment const& judgment, uint32 field_id, ivec2 position, ivec2 size)
{
	constexpr auto JudgeWidth = 200;
	constexpr auto JudgeY = 332;
	constexpr auto TimingWidth = 64;
	constexpr auto TimingY = 316;

	auto const judge_name = format("judgment{}", field_id);
	auto judge_str = string{enum_name(judgment.type)};
	to_upper(judge_str);
	auto const judge_color = judgement_color(judgment.type);
	lib::imgui::begin_window(judge_name.c_str(),
		uvec2{position} + uvec2{static_cast<uint32>(size.x() / 2 - JudgeWidth / 2), JudgeY}, JudgeWidth,
		lib::imgui::WindowStyle::Transparent);
	lib::imgui::text_styled(judge_str, judge_color, 3.0f, lib::imgui::TextAlignment::Center);
	lib::imgui::end_window();

	if (judgment.timing == bms::Score::Timing::None || judgment.timing == bms::Score::Timing::OnTime) return;
	auto const timing_name = format("timing{}", field_id);
	auto timing_str = string{enum_name(judgment.timing)};
	to_upper(timing_str);
	auto const time_color = timing_color(judgment.timing);
	lib::imgui::begin_window(timing_name.c_str(),
		uvec2{position} + uvec2{static_cast<uint32>(size.x() / 2 - TimingWidth / 2), TimingY}, TimingWidth,
		lib::imgui::WindowStyle::Transparent);
	lib::imgui::text_styled(timing_str, time_color, 1.0f, lib::imgui::TextAlignment::Center);
	lib::imgui::end_window();
}

}
