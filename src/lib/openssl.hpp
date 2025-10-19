/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/openssl.hpp:
Wrapper for mio file mapping.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::openssl {

using MD5 = array<byte, 16>;

// Calculate and return the MD5 hash of provided data.
auto md5(span<byte const> data) -> MD5;

// Convert an MD5 hash to a hex string.
[[nodiscard]] auto md5_to_hex(MD5 const&) -> string;

}
