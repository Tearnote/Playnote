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
#include "utils/config.hpp"
#include "lib/imgui.hpp"
#include "gfx/renderer.hpp"
#include "bms/cursor.hpp"
#include "bms/score.hpp"
#include "bms/chart.hpp"

namespace playnote::gfx {

class LegacyPlayfield {
public:
	enum class Side {
		Left,
		Right,
	};

	LegacyPlayfield(int2 position, int length, bms::Playstyle);

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
	static constexpr auto LaneSeparatorColor = float4{0.165f, 0.165f, 0.165f, 1.000f};
	static constexpr auto JudgmentLineHeight = 6;
	static constexpr auto JudgmentLineColor = float4{1.000f, 0.200f, 0.200f, 1.000f};
	static constexpr auto FieldBorderWidth = 2;
	static constexpr auto FieldBorderColor = float4{0.596f, 0.596f, 0.596f, 1.000f};
	static constexpr auto FieldSpacing = 94;
	static constexpr auto NoteHeight = 13;
	static constexpr auto MeasureLineHeight = 1;
	static constexpr auto MeasureLineColor = float4{0.267f, 0.267f, 0.267f, 1.000f};
	static constexpr auto LanePressedMargin = 4;
	static constexpr auto LanePressedColor = float4{1.000f, 1.000f, 1.000f, 1.000f};

	int2 position;
	int length;
	bms::Playstyle playstyle;
	vector<vector<Lane>> fields;
	vector<float> measure_lines;

	[[nodiscard]] auto get_lane(bms::Lane::Type) -> Lane&;
	[[nodiscard]] static auto lane_width(Lane::Visual) -> int;
	[[nodiscard]] static auto lane_background_color(Lane::Visual) -> float4;
	[[nodiscard]] static auto lane_note_color(Lane::Visual) -> float4;
	[[nodiscard]] static auto judgement_color(bms::Score::JudgmentType) -> float4;
	[[nodiscard]] static auto timing_color(bms::Score::Timing) -> float4;

	static auto make_field(bms::Playstyle, Side = Side::Left) -> vector<Lane>;
	static void enqueue_field_border(Renderer::Queue&, int2 position, int2 size);
	static void enqueue_lane(Renderer::Queue&, int2 position, int length, Lane const&, bool left_border, bool pressed);
	static void enqueue_measure_lines(Renderer::Queue&, span<float const> measure_lines, int2 position, int2 size);
	static void enqueue_judgment(bms::Score::Judgment const&, int field_id, int2 position, int2 size);
};

inline LegacyPlayfield::LegacyPlayfield(int2 position, int length, bms::Playstyle playstyle):
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

inline void LegacyPlayfield::enqueue_from_cursor(Renderer::Queue& queue, bms::Cursor const& cursor, bms::Score const& score,
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
	cursor.upcoming_notes(max_distance, [&](bms::Note const& note, auto type, auto, auto distance) {
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
			field_x_advance += lane_width(lane.visual) + (l_idx != static_cast<isize_t>(field.size()) - 1? LaneSeparatorWidth : 0);
		}
		enqueue_measure_lines(queue, measure_lines, {x_advance, position.y()}, {field_x_advance, length});
		enqueue_field_border(queue, {x_advance, position.y()}, {field_x_advance, length});
		auto const judgment = score.get_latest_judgment(idx);
		if (judgment && cursor.get_progress_ns() - judgment->timestamp <= milliseconds{globals::config->get_entry<int>("gameplay", "judgment_timeout")})
			enqueue_judgment(*judgment, idx, {x_advance, position.y()}, {field_x_advance, length});
		x_advance += field_x_advance + FieldSpacing;
	}
}

inline auto LegacyPlayfield::get_lane(bms::Lane::Type type) -> Lane&
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

inline auto LegacyPlayfield::lane_width(Lane::Visual visual) -> int
{
	switch (visual) {
	case Lane::Visual::Odd:     return 40;
	case Lane::Visual::Even:    return 32;
	case Lane::Visual::Scratch: return 72;
	default: PANIC();
	}
}

inline auto LegacyPlayfield::lane_background_color(Lane::Visual visual) -> float4
{
	switch (visual) {
	case Lane::Visual::Scratch:
	case Lane::Visual::Odd:  return {0.000f, 0.000f, 0.000f, 1.000f};
	case Lane::Visual::Even: return {0.035f, 0.035f, 0.035f, 1.000f};
	default: PANIC();
	}
}

inline auto LegacyPlayfield::lane_note_color(Lane::Visual visual) -> float4
{
	switch (visual) {
	case Lane::Visual::Odd:     return {0.800f, 0.800f, 0.800f, 1.000f};
	case Lane::Visual::Even:    return {0.200f, 0.600f, 0.800f, 1.000f};
	case Lane::Visual::Scratch: return {0.800f, 0.200f, 0.200f, 1.000f};
	default: PANIC();
	}
}

inline auto LegacyPlayfield::judgement_color(bms::Score::JudgmentType judge) -> float4
{
	switch (judge) {
	case bms::Score::JudgmentType::PGreat: return {0.533f, 0.859f, 0.961f, 1.000f};
	case bms::Score::JudgmentType::Great:  return {0.980f, 0.863f, 0.380f, 1.000f};
	case bms::Score::JudgmentType::Good:   return {0.796f, 0.576f, 0.191f, 1.000f};
	case bms::Score::JudgmentType::Bad:    return {0.933f, 0.525f, 0.373f, 1.000f};
	case bms::Score::JudgmentType::Poor:   return {0.606f, 0.207f, 0.171f, 1.000f};
	}
}

inline auto LegacyPlayfield::timing_color(bms::Score::Timing timing) -> float4
{
	switch (timing) {
	case bms::Score::Timing::Early: return {0.200f, 0.400f, 0.961f, 1.000f};
	case bms::Score::Timing::Late:  return {0.933f, 0.300f, 0.300f, 1.000f};
	default:                        return {1.000f, 1.000f, 1.000f, 1.000f};
	}
}

inline auto LegacyPlayfield::make_field(bms::Playstyle playstyle, Side side) -> vector<Lane>
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

inline void LegacyPlayfield::enqueue_field_border(Renderer::Queue& queue, int2 position, int2 size)
{
	queue.rect_tl({
		.position = {static_cast<float>(position.x()), static_cast<float>(position.y() + size.y() - JudgmentLineHeight)},
		.color = JudgmentLineColor,
		.depth = 180,
	}, {
		.size = {static_cast<float>(size.x()), JudgmentLineHeight},
	});
	queue.rect_tl({
		.position = {static_cast<float>(position.x() - FieldBorderWidth), static_cast<float>(position.y())},
		.color = FieldBorderColor,
		.depth = 200,
	}, {
		.size = {FieldBorderWidth, static_cast<float>(size.y() + FieldBorderWidth)},
	});
	queue.rect_tl({
		.position = {static_cast<float>(position.x() - FieldBorderWidth), static_cast<float>(size.y())},
		.color = FieldBorderColor,
		.depth = 200,
	}, {
		.size = {static_cast<float>(size.x() + FieldBorderWidth * 2), FieldBorderWidth},
	});
	queue.rect_tl({
		.position = {static_cast<float>(position.x() + size.x()), static_cast<float>(position.y())},
		.color = FieldBorderColor,
		.depth = 200,
	}, {
		.size = {FieldBorderWidth, static_cast<float>(size.y() + FieldBorderWidth)},
	});
}

inline void LegacyPlayfield::enqueue_lane(Renderer::Queue& queue, int2 position, int length, Lane const& lane, bool left_border, bool pressed)
{
	auto const width = lane_width(lane.visual);
	queue.rect_tl({
		.position = {static_cast<float>(position.x()), static_cast<float>(position.y())},
		.color = lane_background_color(lane.visual),
		.depth = 200,
	}, {
		.size = {static_cast<float>(width), static_cast<float>(length)},
	});
	for (auto const& note: lane.notes) {
		if (note.y_pos + note.ln_height < 0.0f) continue;
		auto const y_pos_clipped = max(0.0f, note.y_pos);
		auto const ln_overflow = max(0.0f, -note.y_pos);
		auto const ln_height_clipped = note.ln_height - ln_overflow;
		queue.rect_tl({
			.position = {static_cast<float>(position.x()), position.y() + length - ceil((y_pos_clipped + ln_height_clipped) * length) - NoteHeight},
			.color = lane_note_color(lane.visual),
			.depth = 100,
		}, {
			.size = {static_cast<float>(width), static_cast<float>(NoteHeight + static_cast<int>(ceil(ln_height_clipped * length)))},
		});
	}
	if (left_border) {
		queue.rect_tl({
			.position = {static_cast<float>(position.x() - LaneSeparatorWidth), static_cast<float>(position.y())},
			.color = LaneSeparatorColor,
			.depth = 200,
		}, {
			.size = {LaneSeparatorWidth, static_cast<float>(length - JudgmentLineHeight)},
		});
	}
	if (pressed) {
		queue.rect_tl({
			.position = {static_cast<float>(position.x() + LanePressedMargin), static_cast<float>(position.y() + length + LanePressedMargin * 2 + FieldBorderWidth)},
			.color = LanePressedColor,
			.depth = 80,
		}, {
			.size = {static_cast<float>(width - LanePressedMargin * 2), static_cast<float>(width - LanePressedMargin * 2)},
		});
	}
}

inline void LegacyPlayfield::enqueue_measure_lines(Renderer::Queue& queue, span<float const> measure_lines, int2 position, int2 size)
{
	for (auto y_pos: measure_lines) {
		queue.rect_tl({
			.position = {static_cast<float>(position.x()), static_cast<float>(static_cast<int>(position.y() + size.y() - ceil(y_pos * size.y()) - MeasureLineHeight))},
			.color = MeasureLineColor,
			.depth = 190,
		}, {
			.size = {static_cast<float>(size.x()), static_cast<float>(MeasureLineHeight)},
		});
	}
}

inline void LegacyPlayfield::enqueue_judgment(bms::Score::Judgment const& judgment, int field_id, int2 position, int2 size)
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
		position + int2{size.x() / 2 - JudgeWidth / 2, JudgeY}, JudgeWidth,
		lib::imgui::WindowStyle::Transparent);
	lib::imgui::text_styled(judge_str, judge_color, 3.0f, lib::imgui::TextAlignment::Center);
	lib::imgui::end_window();

	if (judgment.timing == bms::Score::Timing::None || judgment.timing == bms::Score::Timing::OnTime) return;
	auto const timing_name = format("timing{}", field_id);
	auto timing_str = string{enum_name(judgment.timing)};
	to_upper(timing_str);
	auto const time_color = timing_color(judgment.timing);
	lib::imgui::begin_window(timing_name.c_str(),
		position + int2{size.x() / 2 - TimingWidth / 2, TimingY}, TimingWidth,
		lib::imgui::WindowStyle::Transparent);
	lib::imgui::text_styled(timing_str, time_color, 1.0f, lib::imgui::TextAlignment::Center);
	lib::imgui::end_window();
}

}
