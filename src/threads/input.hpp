/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.hpp:
Spinning on the OS message queue as much possible without saturating the CPU.
Only the process's main thread can be the input thread.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "threads/tools.hpp"

namespace playnote::threads {

// Input thread entry point.
void input(Tools&, dev::Window&);

}
