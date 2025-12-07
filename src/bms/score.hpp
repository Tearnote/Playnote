/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
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
		array<ssize_t, enum_count<JudgmentType>()> types;
		array<ssize_t, enum_count<Timing>()> timings;
	};

	// Create a running score for the given chart. The reference is not stored.
	explicit Score(Chart const&);

	// Submit a judgment event to be added to the score.
	void submit_judgment_event(Cursor::JudgmentEvent const&);

	// Return the latest judgment on given playfield.
	[[nodiscard]] auto get_latest_judgment(int field_idx) const -> optional<Judgment>
	{
		return latest_judgement[field_idx];
	}

	// Return the number of playable notes that were already judged.
	[[nodiscard]] auto get_judged_notes() const -> ssize_t { return notes_judged; }

	// Return the note judgments.
	[[nodiscard]] auto get_judge_totals() const -> JudgeTotals { return judge_totals; }

	// Return current combo.
	[[nodiscard]] auto get_combo() const -> ssize_t { return combo; }

	// Return current score.
	[[nodiscard]] auto get_score() const -> ssize_t { return score; }

	// Return accuracy rank.
	[[nodiscard]] auto get_rank() const -> Rank;

private:
	[[maybe_unused]] nanoseconds chart_duration;
	ssize_t notes_judged = 0;
	[[maybe_unused]] ssize_t note_count;
	JudgeTotals judge_totals = {};
	array<optional<Judgment>, 2> latest_judgement = {};
	ssize_t combo = 0;
	ssize_t score = 0;
};

}
