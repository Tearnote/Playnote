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
#include "threads/broadcaster.hpp"

namespace playnote::threads {

void input(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window, fs::path const& song_request);

}
