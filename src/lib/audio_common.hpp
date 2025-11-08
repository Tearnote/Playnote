/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib {

inline constexpr auto ChannelCount = 2zu;

// A single audio sample (frame).
struct Sample {
	float left;
	float right;
};

// The format of an audio sample.
enum class SampleFormat {
	Unknown,
	Float32, // 32-bit IEEE floating point (identical to C++ float type)
	Int16,   // 16-bit signed integer (identical to C++ short type)
	Int24,   // 24-bit signed integer (stored in a 32-bit signed integer, aligned to most significant bits)
};

struct AudioProperties {
	int sampling_rate;
	SampleFormat sample_format;
	int buffer_size;
};

inline auto audio_latency(AudioProperties const& props) -> nanoseconds
{
	return duration_cast<nanoseconds>(
		duration<double>{
			static_cast<double>(props.buffer_size) / static_cast<double>(props.sampling_rate)});
}

}
