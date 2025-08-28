/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

audio/codec.hpp:
A file codec to decode the audio format and convert sample rate.
*/

#pragma once
#include "preamble.hpp"
#include "lib/ffmpeg.hpp"
#include "dev/audio.hpp"

namespace playnote::audio {

class Codec {
public:
	using Output = vector<dev::Sample>;

	static auto process(span<byte const> raw) -> Output;
};

inline auto Codec::process(span<byte const> raw) -> Output
{
	auto const sampling_rate = dev::Audio::get_sampling_rate();
	return lib::ffmpeg::decode_and_resample_file_buffer(raw, sampling_rate);
}

}
