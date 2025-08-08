/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/ffmpeg.hpp:
Wrapper for libav/libswresample audio file decoding.
*/

#pragma once
#include "preamble.hpp"
#include "lib/pipewire.hpp"

namespace playnote::lib::ffmpeg {

struct DecoderOutput_t;
using DecoderOutput = unique_ptr<DecoderOutput_t>;

auto decode_file_buffer(span<byte const> file_contents) -> DecoderOutput;

auto resample_buffer(DecoderOutput&& input, uint32 sampling_rate) -> vector<pw::Sample>;

auto decode_and_resample_file_buffer(span<byte const> file_contents, uint32 sampling_rate) -> vector<pw::Sample>;

}
