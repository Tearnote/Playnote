/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "gfx/playfield.hpp"

#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/config.hpp"

namespace playnote::gfx {

Playfield::Playfield(Transform transform, float height, bms::Cursor const& cursor,
	bms::Score const& score):
	transform{globals::create_transform(transform)}, cursor{cursor}, score{score}
{
	static constexpr auto FieldSpacing = 70.0f;

	// Precalc lane offsets
	auto order = lane_order();
	auto offset = 0.0f;
	auto field_start = 0.0f;
	for (auto lane_idx: order) {
		if (lane_idx == -1) {
			fields.emplace_back(field_start, offset - field_start);
			offset += FieldSpacing;
			field_start = offset;
			continue;
		}
		auto const lane_type = bms::Lane::Type{lane_idx};
		lane_offsets[lane_idx] = globals::create_child_transform(this->transform, offset, 0.0f);
		auto const note_type = lane_to_note_type(lane_type);
		auto const width = lane_width(note_type);
		offset += width;
	}
	fields.emplace_back(field_start, offset - field_start);
	lane_offsets[+bms::Lane::Type::MeasureLine] = globals::create_child_transform(this->transform);
	size = {offset, height};
}

void Playfield::enqueue(Renderer::Queue& queue, float scroll_speed, nanoseconds offset)
{
	static constexpr auto JudgmentLineHeight = 4.5f;
	static constexpr auto LanePressedMargin = 3.0f;

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
	for (auto&& note: cursor.upcoming_notes(max_distance, offset, true)) {
		auto& lane = lanes[+note.lane];
		auto existing = find(lane, note.lane_idx, &Note::lane_idx);
		if (existing == lane.end()) {
			lane.emplace_back(Note{
				.type = lane_to_note_type(note.lane),
				.lane_idx = note.lane_idx,
				.transform = globals::create_child_transform(lane_offsets[+note.lane],
					0.0f, (1.0f - (note.distance / max_distance)) * size.y()),
				.ln_height = note.note.type_is<bms::Note::LN>()?
					note.note.params<bms::Note::LN>().height / max_distance * size.y() :
					0.0f,
			});
		} else {
			existing->transform->position.y() = (1.0f - (note.distance / max_distance)) * size.y();
		}
	}

	// Enqueue lane backgrounds and hold display
	for (auto [idx, lane, lane_transform]: views::zip(views::iota(0), lanes, lane_offsets)) {
		auto const lane_type = bms::Lane::Type{idx};
		if (!lane_transform || lane_type == bms::Lane::Type::MeasureLine) continue;
		auto const width = lane_width(lane_to_note_type(lane_type));
		queue.rect_tl({
			.position = lane_transform->global_position(),
			.color = lane_background_color(lane_type),
			.depth = 200,
		}, {
			.size = {width, size.y()},
		});
		if (cursor.is_pressed(lane_type)) {
			queue.rect_tl({
				.position = lane_transform->global_position() + float2{LanePressedMargin, size.y() + LanePressedMargin * 2.0f},
				.color = {1.000f, 1.000f, 1.000f, 1.000f},
				.depth = 80,
			}, {
				.size = {width - LanePressedMargin * 2.0f, width - LanePressedMargin * 2.0f},
			});
		}
	}

	// Enqueue judgment line and judgment display
	for (auto [idx, field]: fields | views::enumerate) {
		queue.rect_tl({
			.position = transform->global_position() + float2{field.start, size.y() - JudgmentLineHeight},
			.color = {1.000f, 0.200f, 0.200f, 1.000f},
			.depth = 180,
		}, {
			.size = {field.length, JudgmentLineHeight},
		});

		constexpr auto JudgeWidth = 200.0f;
		constexpr auto JudgeY = 249.0f;
		constexpr auto TimingWidth = 64.0f;
		constexpr auto TimingY = 237.0f;

		auto const judgment = score.get_latest_judgment(idx);
		if (judgment && cursor.get_progress_ns() - judgment->timestamp <= milliseconds{playnote::globals::config->get_entry<int>("gameplay", "judgment_timeout")}) {
			auto const judge_name = format("judgment{}", idx);
			auto judge_str = string{enum_name(judgment->type)};
			to_upper(judge_str);
			auto const judge_color = judgement_color(judgment->type);
			lib::imgui::begin_window(judge_name.c_str(),
				int2{queue.logical_to_physical(transform->global_position() + float2{field.start, 0.0f} + float2{field.length / 2.0f - JudgeWidth / 2.0f, JudgeY})},
				JudgeWidth,
				lib::imgui::WindowStyle::Transparent);
			lib::imgui::text_styled(judge_str, judge_color, 3.0f, lib::imgui::TextAlignment::Center);
			lib::imgui::end_window();

			if (judgment->timing == bms::Score::Timing::None || judgment->timing == bms::Score::Timing::OnTime) continue;
			auto const timing_name = format("timing{}", idx);
			auto timing_str = string{enum_name(judgment->timing)};
			to_upper(timing_str);
			auto const time_color = timing_color(judgment->timing);
			lib::imgui::begin_window(timing_name.c_str(),
				int2{queue.logical_to_physical(transform->global_position() + float2{field.start, 0.0f} + float2{field.length / 2.0f - TimingWidth / 2.0f, TimingY})},
				TimingWidth,
				lib::imgui::WindowStyle::Transparent);
			lib::imgui::text_styled(timing_str, time_color, 1.0f, lib::imgui::TextAlignment::Center);
			lib::imgui::end_window();
		}
	}

	// Enqueue visible notes
	for (auto const& lane: lanes) {
		for (auto const& note: lane) {
			auto const size = note_size(note.type) + float2{0.0f, note.ln_height};
			auto const ln_overflow = max(0.0f, note.transform->position.y() - this->size.y());
			queue.rect_tl({
				.position = note.transform->global_position() - float2{0.0f, size.y()},
				.color = note_color(note.type),
				.depth = note.type == Note::Type::MeasureLine? 190 : 100,
			}, {
				.size = size - float2{0.0f, ln_overflow},
			});
		}
	}
}

auto Playfield::lane_order() const -> span<ssize_t const>
{
	static constexpr auto LaneOrder5K = to_array<ssize_t>({
		+bms::Lane::Type::P1_KeyS,
		+bms::Lane::Type::P1_Key1,
		+bms::Lane::Type::P1_Key2,
		+bms::Lane::Type::P1_Key3,
		+bms::Lane::Type::P1_Key4,
		+bms::Lane::Type::P1_Key5,
	});
	static constexpr auto LaneOrder7K = to_array<ssize_t>({
		+bms::Lane::Type::P1_KeyS,
		+bms::Lane::Type::P1_Key1,
		+bms::Lane::Type::P1_Key2,
		+bms::Lane::Type::P1_Key3,
		+bms::Lane::Type::P1_Key4,
		+bms::Lane::Type::P1_Key5,
		+bms::Lane::Type::P1_Key6,
		+bms::Lane::Type::P1_Key7,
	});
	static constexpr auto LaneOrder10K = to_array<ssize_t>({
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
	static constexpr auto LaneOrder14K = to_array<ssize_t>({
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

auto Playfield::lane_to_note_type(bms::Lane::Type lane) const -> Note::Type
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

auto Playfield::lane_background_color(bms::Lane::Type type) const -> float4
{
	auto const note_type = lane_to_note_type(type);
	switch (note_type) {
		case Note::Type::Odd:     return {0.035f, 0.035f, 0.035f, 1.000f};
		case Note::Type::Scratch:
		case Note::Type::Even:    return {0.000f, 0.000f, 0.000f, 1.000f};
		default: PANIC();
	}
}

auto Playfield::lane_width(Note::Type type) const -> float
{
	switch (type) {
		case Note::Type::Odd:
			return 30.0f;
		case Note::Type::Even:
			return 24.0f;
		case Note::Type::Scratch:
			return 54.0f;
		default: PANIC();
	}
}

auto Playfield::note_color(Note::Type type) const -> float4
{
	switch (type) {
	case Note::Type::Odd:         return {0.800f, 0.800f, 0.800f, 1.000f};
	case Note::Type::Even:        return {0.200f, 0.600f, 0.800f, 1.000f};
	case Note::Type::Scratch:     return {0.800f, 0.200f, 0.200f, 1.000f};
	case Note::Type::MeasureLine: return {0.267f, 0.267f, 0.267f, 1.000f};
	default: PANIC();
	}
}

auto Playfield::note_size(Note::Type type) const -> float2
{
	switch (type) {
	case Note::Type::Odd:         return {30.0f, 10.0f};
	case Note::Type::Even:        return {24.0f, 10.0f};
	case Note::Type::Scratch:     return {54.0f, 10.0f};
	case Note::Type::MeasureLine: return {size.x(), 0.75f};
	default: PANIC();
	}
}

auto Playfield::judgement_color(bms::Score::JudgmentType judge) const -> float4
{
	switch (judge) {
	case bms::Score::JudgmentType::PGreat: return {0.533f, 0.859f, 0.961f, 1.000f};
	case bms::Score::JudgmentType::Great:  return {0.980f, 0.863f, 0.380f, 1.000f};
	case bms::Score::JudgmentType::Good:   return {0.796f, 0.576f, 0.191f, 1.000f};
	case bms::Score::JudgmentType::Bad:    return {0.933f, 0.525f, 0.373f, 1.000f};
	case bms::Score::JudgmentType::Poor:   return {0.606f, 0.207f, 0.171f, 1.000f};
	}
}

auto Playfield::timing_color(bms::Score::Timing timing) const -> float4
{
	switch (timing) {
	case bms::Score::Timing::Early: return {0.200f, 0.400f, 0.961f, 1.000f};
	case bms::Score::Timing::Late:  return {0.933f, 0.300f, 0.300f, 1.000f};
	default:                        return {1.000f, 1.000f, 1.000f, 1.000f};
	}
}

}
