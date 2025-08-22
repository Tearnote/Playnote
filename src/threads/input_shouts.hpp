/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input_shouts.hpp:
Shouts that can be spawned by the input thread. Typically messages from the OS message queue.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"

namespace playnote::threads {

using ChartLoadRequest = fs::path;

struct KeyInput {
	nanoseconds timestamp;
	dev::Window::KeyCode keycode;
	bool state;
};

}
