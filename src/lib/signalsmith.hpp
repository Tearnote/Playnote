/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/signalsmith.hpp:
Wrapper for signalsmith-basics, a DSP library.
*/

#pragma once
#include "signalsmith-basics/limiter.h"
#include "preamble.hpp"
#include "assert.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::dsp {

// A simple lookahead signal limiter for the [-1.0, 1.0] range.
class Limiter {
public:
	// Initialize with provided parameters.
	Limiter(uint32 sampling_rate, milliseconds attack, milliseconds hold, milliseconds release);

	// Process a single sample.
	auto process(Sample in) noexcept -> Sample;

private:
	signalsmith::basics::LimiterDouble limiter;
};

inline Limiter::Limiter(uint32 sampling_rate, milliseconds attack, milliseconds hold,
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
