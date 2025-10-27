/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
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
