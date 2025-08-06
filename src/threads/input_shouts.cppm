/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input_shouts.cppm:
Shouts that can be spawned by the input thread. Typically messages from the OS message queue.
*/

module;
#include "preamble.hpp"

export module playnote.threads.input_shouts;

namespace playnote::threads {

export using ChartLoadRequest = fs::path;

}
