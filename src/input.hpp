/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
except according to those terms.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "utils/broadcaster.hpp"

namespace playnote {

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
	int duplicate; // Initially 0, incremented if a duplicate GUID is found
	auto operator==(ControllerID const&) const -> bool = default;
};

// A controller button event.
struct ButtonInput {
	ControllerID controller;
	nanoseconds timestamp; // Time since application start
	int button;
	bool state;
};

// A controller axis event.
struct AxisInput {
	ControllerID controller;
	nanoseconds timestamp; // Time since application start
	int axis;
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
void input_thread(Broadcaster& broadcaster, Barriers<2>& barriers, dev::Window&);

}
