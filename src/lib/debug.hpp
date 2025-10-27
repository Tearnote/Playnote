/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble.hpp"

namespace playnote::lib::dbg {

// Register a handler to make all assert failures throw.
void set_assert_handler();

// Open the console window and attach standard outputs to it.
// https://github.com/ocaml/ocaml/issues/9252#issuecomment-576383814
void attach_console();

}
