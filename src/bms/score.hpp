/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/score.hpp:
Accumulator of note hit events, judging them and providing totals.
*/

#pragma once
#include "preamble.hpp"
#include "bms/cursor.hpp"
#include "bms/chart.hpp"

namespace playnote::bms {

class Score {
public:
	static constexpr auto   PGreatWindow =  18ms;
	static constexpr auto    GreatWindow =  36ms;
	static constexpr auto     GoodWindow = 120ms;
	static constexpr auto      BadWindow = 240ms;
	static constexpr auto LNEarlyRelease = 120ms;

	enum class Rank {
		AAA, AA, A,
		B, C, D, E, F,
	};

	enum class JudgmentType {
		PGreat,
		Great,
		Good,
		Bad,
		Poor,
	};

	enum class Timing {
		None, // Misses
		Early,
		OnTime,
		Late,
	};

	struct Judgment {
		JudgmentType type;
		Timing timing;
		nanoseconds timestamp;
	};

	struct JudgeTotals {
		array<isize, enum_count<JudgmentType>()> types;
		array<isize, enum_count<Timing>()> timings;
	};

	// Create a running score for the given chart. The reference is not stored.
	explicit Score(Chart const&);

	// Submit a judgment event to be added to the score.
	void submit_judgment_event(Cursor::JudgmentEvent const&);

	// Return the latest judgment on given playfield.
	[[nodiscard]] auto get_latest_judgment(uint32 field_idx) const -> optional<Judgment>
	{
		return latest_judgement[field_idx];
	}

	// Return the number of playable notes that were already judged.
	[[nodiscard]] auto get_judged_notes() const -> isize { return notes_judged; }

	// Return the note judgments.
	[[nodiscard]] auto get_judge_totals() const -> JudgeTotals { return judge_totals; }

	// Return current combo.
	[[nodiscard]] auto get_combo() const -> isize { return combo; }

	// Return current score.
	[[nodiscard]] auto get_score() const -> isize { return score; }

	// Return accuracy rank.
	[[nodiscard]] auto get_rank() const -> Rank;

private:
	[[maybe_unused]] nanoseconds chart_duration;
	isize notes_judged = 0;
	[[maybe_unused]] isize note_count;
	JudgeTotals judge_totals = {};
	array<optional<Judgment>, 2> latest_judgement = {};
	isize combo = 0;
	isize score = 0;
};

inline Score::Score(Chart const& chart):
	chart_duration{chart.metadata.chart_duration},
	note_count{chart.metadata.note_count}
{}

inline void Score::submit_judgment_event(Cursor::JudgmentEvent const& event)
{
	if (event.type == Cursor::JudgmentEvent::Type::LNStart) return; // Currently unused

	auto judgment_type = JudgmentType{};
	auto timing_type = Timing{};
	if ((event.type == Cursor::JudgmentEvent::Type::Note && !event.timing) ||
		(event.type == Cursor::JudgmentEvent::Type::LN && *event.release_timing < -LNEarlyRelease))
	{
		// Handle miss
		combo = 0;
		judgment_type = JudgmentType::Poor;
		timing_type = Timing::None;
		notes_judged += 1;
	} else {
		// Handle hit
		auto const timing = abs(*event.timing);
		judgment_type = [&] {
			if (timing <= PGreatWindow) return JudgmentType::PGreat;
			if (timing <=  GreatWindow) return JudgmentType::Great;
			if (timing <=   GoodWindow) return JudgmentType::Good;
			return JudgmentType::Bad;
		}();
		score += [&] {
			switch (judgment_type) {
			case JudgmentType::PGreat: return 2;
			case JudgmentType::Great:  return 1;
			default: return 0;
			}
		}();
		if (judgment_type == JudgmentType::Bad)
			combo = 0;
		else
			combo += 1;
		if (judgment_type == JudgmentType::PGreat)
			timing_type = Timing::OnTime;
		else if (*event.timing < 0s)
			timing_type = Timing::Early;
		else
			timing_type = Timing::Late;
		notes_judged += 1;
	}
	judge_totals.types[+judgment_type] += 1;
	judge_totals.timings[+timing_type] += 1;

	auto const field_id = +event.lane < +Lane::Type::P2_Key1? 0 : 1;
	latest_judgement[field_id] = Judgment{
		.type = judgment_type,
		.timing = timing_type,
		.timestamp = event.timestamp,
	};
}

inline auto Score::get_rank() const -> Rank
{
	if (notes_judged == 0) return Rank::AAA;
	auto const acc = static_cast<double>(score) / static_cast<double>(notes_judged * 2);
	if (acc >= 8.0 / 9.0) return Rank::AAA;
	if (acc >= 7.0 / 9.0) return Rank::AA;
	if (acc >= 6.0 / 9.0) return Rank::A;
	if (acc >= 5.0 / 9.0) return Rank::B;
	if (acc >= 4.0 / 9.0) return Rank::C;
	if (acc >= 3.0 / 9.0) return Rank::D;
	if (acc >= 2.0 / 9.0) return Rank::E;
	return Rank::F;
}

}
