/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/zstd.cpp:
Implementation file for lib/zstd.hpp.
*/

#include "lib/zstd.hpp"

#include <zstd.h>
#include "preamble.hpp"

namespace playnote::lib::zstd {

// Error handling helpers

auto ret_check(size_t ret) -> size_t
{
	if (ZSTD_isError(ret)) throw runtime_error_fmt("zstd error: {}", ZSTD_getErrorName(ret));
	return ret;
}

auto compress(span<byte const> data) -> vector<byte>
{
	auto result = vector<byte>{};
	result.resize(ZSTD_compressBound(data.size()));
	auto const size = ret_check(ZSTD_compress(result.data(), result.size(), data.data(), data.size(), 9));
	result.resize(size);
	return result;
}

auto decompress(span<byte const> data) -> vector<byte>
{
	auto result = vector<byte>{};
	result.resize(ret_check(ZSTD_getFrameContentSize(data.data(), data.size())));
	ret_check(ZSTD_decompress(result.data(), result.size(), data.data(), data.size()));
	return result;
}

}
