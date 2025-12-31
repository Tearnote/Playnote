/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <cstdint>
#include <cstddef>

namespace playnote {

using std::int8_t;
using std::int16_t;
static_assert(sizeof(int) == 4);
using std::int64_t;
using std::uint8_t;
using std::uint16_t;
using uint = std::uint32_t;
using std::uint64_t;
using std::size_t;
using ssize_t = decltype(0z);
using std::byte;

}
