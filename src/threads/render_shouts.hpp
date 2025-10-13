/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render_shouts.hpp:
Shouts that can be spawned by the render thread.
*/

#pragma once
#include "preamble.hpp"
#include "threads/input_shouts.hpp"

namespace playnote::threads {

struct RegisterInputQueue {
	weak_ptr<mpmc_queue<UserInput>> queue;
};

struct UnregisterInputQueue {
	weak_ptr<mpmc_queue<UserInput>> queue;
};

}
