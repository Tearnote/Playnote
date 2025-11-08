/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "signalsmith-basics/limiter.h"
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::dsp {

// A simple lookahead signal limiter for the [-1.0, 1.0] range.
class Limiter {
public:
	// Initialize with provided parameters.
	Limiter(int sampling_rate, milliseconds attack, milliseconds hold, milliseconds release);

	// Process a single sample.
	auto process(Sample in) noexcept -> Sample;

private:
	signalsmith::basics::LimiterDouble limiter;
};

inline Limiter::Limiter(int sampling_rate, milliseconds attack, milliseconds hold,
	milliseconds release):
	limiter{attack.count()}
{
	limiter.outputLimit = 1.0;
	limiter.attackMs = attack.count();
	limiter.holdMs = hold.count();
	limiter.releaseMs = release.count();
	limiter.smoothingStages = 4;
	ASSUME(limiter.configure(sampling_rate, 1, 2, 2));
}

inline auto Limiter::process(Sample in) noexcept -> Sample
{
	auto out = Sample{};
	auto in_buf = to_array({to_array({in.left}), to_array({in.right})});
	auto out_buf = decltype(in_buf){};
	limiter.process(in_buf, out_buf, 1);
	out.left = out_buf[0][0];
	out.right = out_buf[1][0];
	return out;
}

}
