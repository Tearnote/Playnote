/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/ffmpeg.hpp:
Wrapper for libav/libswresample audio file decoding.
*/

#pragma once
#include "preamble.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::ffmpeg {

// Opaque type for raw decoder output, unusable before being resampled to a known format.
struct DecoderOutput_t;
using DecoderOutput = DecoderOutput_t*;

// Decode an audio file from a buffer into uncompressed audio data. The returned output must be
// consumed by resample_buffer() to free the underlying resources.
// Throws runtime_error if ffmpeg throws.
auto decode_file_buffer(span<byte const> file_contents) -> DecoderOutput;

// Resample decoded audio to a known format.
// Throws runtime_error if ffmpeg throws.
auto resample_buffer(DecoderOutput&& input, uint32 sampling_rate) -> vector<Sample>;

// Perform both decoding and resampling in one step.
// Throws runtime_error if ffmpeg throws.
auto decode_and_resample_file_buffer(span<byte const> file_contents, uint32 sampling_rate) -> vector<Sample>;

// Encode audio samples to an OGG Vorbis buffer.
// Throws runtime_error if ffmpeg throws.
auto encode_as_ogg(span<Sample const> samples, uint32 sampling_rate) -> vector<byte>;

}
