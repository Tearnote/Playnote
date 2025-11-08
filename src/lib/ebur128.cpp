/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#include "lib/ebur128.hpp"

#include "ebur128.h"
#include "preamble.hpp"
#include "lib/audio_common.hpp"

namespace playnote::lib::ebur128 {

struct Context_t: ebur128_state {};

// Helper functions for error handling

template<typename T>
static auto ptr_check(T* ptr, string_view message = "libebur128 error") -> T*
{
	if (!ptr) throw system_error_fmt("{}", message);
	return ptr;
}

static void ret_check(int ret, string_view message = "libebur128 error")
{
	if (ret != EBUR128_SUCCESS) throw system_error_fmt("{}: #{}", message, ret);
}

auto init(int sampling_rate) -> Context
{
	return Context{static_cast<Context_t*>(ptr_check(ebur128_init(2, sampling_rate, EBUR128_MODE_I)))};
}

void detail::ContextDeleter::operator()(Context_t* ctx) noexcept
{
	ebur128_destroy(reinterpret_cast<ebur128_state**>(&ctx));
}

void add_frames(Context& ctx, span<Sample const> frames)
{
	ret_check(ebur128_add_frames_float(ctx.get(), reinterpret_cast<float const*>(frames.data()), frames.size()));
}

auto get_loudness(Context& ctx) -> double
{
	auto result = 0.0;
	ret_check(ebur128_loudness_global(ctx.get(), &result));
	return result;
}

}
