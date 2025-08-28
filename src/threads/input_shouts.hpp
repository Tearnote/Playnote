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

// A request for a chart to be loaded and played.
struct ChartRequest {
	fs::path domain;
	string filename;
};

// A player keyboard input event.
struct KeyInput {
	using Code = dev::Window::KeyCode;
	nanoseconds timestamp; // Time since application start
	Code code;
	bool state; // true = pressed, false = released
};

}
