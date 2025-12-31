/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "utils/logger.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::ffmpeg {

// Opaque type for raw decoder output, unusable before being resampled to a known format.
struct DecoderOutput_t;
using DecoderOutput = DecoderOutput_t*;

// Set the logger category for ffmpeg to use on the current thread. If not called, will log
// to the global category.
void set_thread_log_category(Logger::Category);

// Decode an audio file from a buffer into uncompressed audio data. The returned output must be
// consumed by resample_buffer() to free the underlying resources.
// Throws runtime_error if ffmpeg throws.
auto decode_file_buffer(span<byte const> file_contents) -> DecoderOutput;

// Resample decoded audio to a known format.
// Throws runtime_error if ffmpeg throws.
auto resample_buffer(DecoderOutput&& input, int sampling_rate) -> vector<Sample>;

// Perform both decoding and resampling in one step.
// Throws runtime_error if ffmpeg throws.
auto decode_and_resample_file_buffer(span<byte const> file_contents, int sampling_rate) -> vector<Sample>;

// Encode audio samples to an OGG Vorbis buffer.
// Throws runtime_error if ffmpeg throws.
auto encode_as_ogg(span<Sample const> samples, int sampling_rate) -> vector<byte>;

// Encode audio samples to an Opus buffer.
// Throws runtime_error if ffmpeg throws.
auto encode_as_opus(span<Sample const> samples, int sampling_rate) -> vector<byte>;

}
