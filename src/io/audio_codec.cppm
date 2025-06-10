/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/audio_codec.cppm:
The file codec to decode the audio format and convert sample rate.
*/

module;
#include "macros/assert.hpp"

export module playnote.io.audio_codec;

import playnote.preamble;
import playnote.lib.ffmpeg;
import playnote.dev.audio;

namespace playnote::io {

export class AudioCodec {
public:
	using Output = vector<dev::Sample>;

	static auto process(span<byte const> raw) -> Output;
};

auto AudioCodec::process(span<byte const> raw) -> Output
{
	auto const sampling_rate = dev::Audio::get_sampling_rate();
	ASSERT(sampling_rate != 0);
	return lib::ffmpeg::decode_and_resample_file_buffer(raw, sampling_rate);
}

}
