/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/input.hpp:
Main thread. Spins on the OS message queue as much possible without saturating the CPU.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "threads/tools.hpp"

namespace playnote::threads {

// File drop event.
struct FileDrop {
	vector<fs::path> paths;
};

// A player keyboard input event.
struct KeyInput {
	using Code = dev::Window::KeyCode;
	nanoseconds timestamp; // Time since application start
	Code code;
	bool state; // true = pressed, false = released
};

// Unique identifier for a controller. Can be persisted between sessions.
struct ControllerID {
	id guid; // Hash of the GUID
	uint32 duplicate; // Initially 0, incremented if a duplicate GUID is found
	auto operator==(ControllerID const&) const -> bool = default;
};

// A controller button event.
struct ButtonInput {
	ControllerID controller;
	nanoseconds timestamp; // Time since application start
	uint32 button;
	bool state;
};

// A controller axis event.
struct AxisInput {
	ControllerID controller;
	nanoseconds timestamp; // Time since application start
	uint32 axis;
	float value;
};

// Any user input event.
using UserInput = variant<KeyInput, ButtonInput, AxisInput>;

// A request to add a queue to push input events into.
struct RegisterInputQueue {
	weak_ptr<spsc_queue<UserInput>> queue;
};

// A request to remove a previously added input queue.
struct UnregisterInputQueue {
	weak_ptr<spsc_queue<UserInput>> queue;
};

// Input thread entry point.
void input(Tools&, dev::Window&);

}
