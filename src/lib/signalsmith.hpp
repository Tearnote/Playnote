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

}
