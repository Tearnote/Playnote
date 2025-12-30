/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
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

auto compress(span<byte const> data, CompressionLevel level) -> vector<byte>
{
	auto result = vector<byte>{};
	result.resize(ZSTD_compressBound(data.size()));
	auto const size = ret_check(ZSTD_compress(result.data(), result.size(), data.data(), data.size(), +level));
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
