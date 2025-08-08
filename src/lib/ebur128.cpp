/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/ebur128.cpp:
Implementation file for lib/ebur128.hpp.
*/

#include "lib/ebur128.hpp"

#include "ebur128.h"
#include "preamble.hpp"
#include "lib/pipewire.hpp"

namespace playnote::lib::ebur128 {

struct Context_t: ebur128_state {};

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

auto init(uint32 sampling_rate) -> Context
{
	return static_cast<Context>(ptr_check(ebur128_init(2, sampling_rate, EBUR128_MODE_I)));
}

void cleanup(Context ctx) noexcept { ebur128_destroy(reinterpret_cast<ebur128_state**>(&ctx)); }

void add_frames(Context ctx, span<pw::Sample const> frames)
{
	ret_check(ebur128_add_frames_float(ctx, reinterpret_cast<float const*>(frames.data()), frames.size()));
}

auto get_loudness(Context ctx) -> double
{
	auto result = 0.0;
	ret_check(ebur128_loudness_global(ctx, &result));
	return result;
}

}
