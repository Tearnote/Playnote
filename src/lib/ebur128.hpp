/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/ebur128.hpp:
Wrapper for libebur128 psychoacoustic audio volume analysis.
*/

#pragma once
#include "preamble.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::ebur128 {

// Library context for storing accumulated loudness information.
struct Context_t;
using Context = Context_t*;

// Create a context to accumulate audio frames. Call cleanup() when finished.
// Throws if libebur128 throws.
auto init(uint32 sampling_rate) -> Context;

// Destroy a context once finished.
void cleanup(Context ctx) noexcept;

// Process audio frames. They can all be added at once, or in chunks to save memory.
// Throws if libebur128 throws.
void add_frames(Context ctx, span<Sample const> frames);

// After all frames were added, call this to get the loudness of the entire audio in LUFS
// (Loudness Units relative to Full Scale).
// Throws if libebur128 throws.
auto get_loudness(Context ctx) -> double;

}
