/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

io/audio_codec.cppm:
The file codec to decode the audio format and convert sample rate.
*/

export module playnote.io.audio_codec;

import playnote.preamble;
import playnote.lib.pipewire;
import playnote.lib.ffmpeg;

namespace playnote::io {

export class AudioCodec {
public:
	using Sample = lib::pw::Sample;
	using Output = vector<Sample>;

	static auto process(span<byte const> raw) -> Output;
};

auto AudioCodec::process(span<byte const> raw) -> Output
{
	return lib::ffmpeg::decode_and_resample_file_buffer(raw, 48000);
}

}
