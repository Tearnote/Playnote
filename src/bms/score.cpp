/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "bms/score.hpp"

#include "preamble.hpp"

namespace playnote::bms {

Score::Score(Chart const& chart):
	chart_duration{chart.metadata.chart_duration},
	note_count{chart.metadata.note_count}
{}

void Score::submit_judgment_event(Cursor::JudgmentEvent const& event)
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

auto Score::get_rank() const -> Rank
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
