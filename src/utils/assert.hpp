/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "libassert/assert.hpp" // IWYU pragma: export

// ASSERT() - checked in both debug and release
// ASSUME() - checked in debug only
// PANIC() - unconditional failure in both debug and release
// _VAL suffix - returns the result of the tested expression.
//     For comparisons, the left side is returned.
