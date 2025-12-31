/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::ebur128 {

struct Context_t;

namespace detail {
struct ContextDeleter {
	static void operator()(Context_t* ctx) noexcept;
};
}

// Library context for storing accumulated loudness information.
using Context = unique_resource<Context_t*, detail::ContextDeleter>;

// Create a context to accumulate audio frames. Call cleanup() when finished.
// Throws if libebur128 throws.
auto init(int sampling_rate) -> Context;

// Process audio frames. They can all be added at once, or in chunks to save memory.
// Throws if libebur128 throws.
void add_frames(Context& ctx, span<Sample const> frames);

// After all frames were added, call this to get the loudness of the entire audio in LUFS
// (Loudness Units relative to Full Scale).
// Throws if libebur128 throws.
auto get_loudness(Context& ctx) -> double;

}
