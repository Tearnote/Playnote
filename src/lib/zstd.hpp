/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::zstd {

enum class CompressionLevel {
	Normal = 9,
	Ultra = 22,
};

// Compress arbitrary data.
auto compress(span<byte const> data, CompressionLevel = CompressionLevel::Normal) -> vector<byte>;

// Decompress arbitrary data.
auto decompress(span<byte const> data) -> vector<byte>;

}
