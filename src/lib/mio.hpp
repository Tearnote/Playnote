/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#if defined(_WIN32) && !defined(NOMINMAX) // mio includes windows.h without using this macro
#define NOMINMAX
#endif
#include "mio/mmap.hpp"
#include "preamble.hpp"

namespace playnote::lib::mio {

using ReadMapping = ::mio::basic_mmap_source<byte>;

}
