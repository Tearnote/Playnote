/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/audio_common.hpp:
Definitions used by every audio backend.
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
	uint32 sampling_rate;
	SampleFormat sample_format;
	uint32 buffer_size;
};

inline auto audio_latency(AudioProperties const& props) -> nanoseconds
{
	return duration_cast<nanoseconds>(
		duration<double>{
			static_cast<double>(props.buffer_size) / static_cast<double>(props.sampling_rate)});
}

}
