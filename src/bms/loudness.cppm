/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

bms/loudness.cppm:
Measurer of a BMS song's loudness. This can be used to normalize songs by applying reverse gain.
*/

module;
#include "macros/assert.hpp"

export module playnote.bms.loudness;

import playnote.preamble;
import playnote.lib.pipewire;
import playnote.lib.ebur128;
import playnote.io.audio_codec;
import playnote.bms.chart;

namespace playnote::bms {

namespace r128 = lib::ebur128;
namespace pw = lib::pw;

export [[nodiscard]] auto measure_loudness(bms::Chart& chart) -> double
{
	constexpr auto BufferSize = 4096zu / sizeof(pw::Sample);
	ASSERT(chart.at_start());

	auto ctx = r128::init(io::AudioCodec::sampling_rate);
	auto buffer = vector<pw::Sample>{};
	buffer.reserve(BufferSize);

	auto processing = true;
	while (processing) {
		for (auto i: views::iota(0zu, BufferSize)) {
			auto& sample = buffer.emplace_back();
			processing = !chart.advance_one_sample([&](auto new_sample) {
				sample.left += new_sample.left;
				sample.right += new_sample.right;
			});
			if (!processing) break;
		}

		r128::add_frames(ctx, buffer);
		buffer.clear();
	}

	auto const result = r128::get_loudness(ctx);

	r128::cleanup(ctx);
	chart.restart();
	return result;
}

export [[nodiscard]] auto lufs_to_gain(double lufs) noexcept -> float
{
	constexpr auto LufsTarget = -14.0;
	auto const db_from_target = LufsTarget - lufs;
	auto const amplitude_ratio = pow(10.0, db_from_target / 20.0);
	return static_cast<float>(amplitude_ratio);
}

}
