/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/debug.hpp:
Wrapper for OS-specific debugging enablement.
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
