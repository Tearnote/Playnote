/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/ebur128.cppm:
Wrapper for libebur128 psychoacoustic audio volume analysis.
*/

module;
#include "ebur128.h"

export module playnote.lib.ebur128;

import playnote.preamble;
import playnote.lib.pipewire;

namespace playnote::lib {

// Library context for storing accumulated loudness information.
export using Context = ebur128_state*;

// Error checking wrappers for libebur128 functions.
template<typename T>
auto ptr_check(T* ptr, string_view message = "libebur128 error") -> T*
{
	if (!ptr) throw system_error_fmt("{}", message);
	return ptr;
}

void ret_check(int ret, string_view message = "libebur128 error")
{
	if (ret != EBUR128_SUCCESS) throw system_error_fmt("{}: #{}", message, ret);
}

// Create a context to accumulate audio frames. Call cleanup() when finished.
// Throws if libebur128 throws.
export auto init(uint32 sampling_rate) -> Context
{
	return ptr_check(ebur128_init(2, sampling_rate, EBUR128_MODE_I));
}

// Destroy a context once finished.
export void cleanup(Context ctx) noexcept { ebur128_destroy(&ctx); }

// Process audio frames. They can all be added at once, or in chunks to save memory.
// Throws if libebur128 throws.
export void add_frames(Context ctx, span<pw::Sample const> frames)
{
	ret_check(ebur128_add_frames_float(ctx, reinterpret_cast<float const*>(frames.data()), frames.size()));
}

// After all frames were added, call this to get the loudness of the entire audio in LUFS
// (Loudness Units relative to Full Scale).
// Throws if libebur128 throws.
export auto get_loudness(Context ctx) -> double
{
	auto result = 0.0;
	ret_check(ebur128_loudness_global(ctx, &result));
	return result;
}

}
