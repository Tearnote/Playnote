/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/audio_codec.cppm:
The file codec to decode the audio format and convert sample rate.
*/

export module playnote.io.audio_codec;

import playnote.preamble;

namespace playnote::io {

export class AudioCodec {
public:
	struct Sample {
		float left;
		float right;
	};
	using Output = vector<Sample>;

	static auto process(span<char const> raw) -> Output;
};

auto AudioCodec::process(span<char const> raw) -> Output
{
	//todo
	return {};
}

}
