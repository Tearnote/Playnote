/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/zstd.hpp:
Wrapper for zstd lossless compression library.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::zstd {

// Compress arbitrary data.
auto compress(span<byte const> data) -> vector<byte>;

// Decompress arbitrary data.
auto decompress(span<byte const> data) -> vector<byte>;

}
