/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once

#include "preamble.hpp"
#include "utils/logger.hpp"
#include "input.hpp"

namespace playnote::dev {

// Handler of controller connect/disconnect events. Dispatches controller input events.
class ControllerDispatcher {
public:
	using ControllerEvent = variant<ButtonInput, AxisInput>;

	// Initialize the dispatcher. Only one can exist at a time, since it handles all controllers
	// globally.
	explicit ControllerDispatcher(Logger::Category);

	// Update state of all connected controllers. If a button state or axis value changed since
	// the last poll, a corresponding event is generated.
	auto poll() -> generator<ControllerEvent>;

private:
	InstanceLimit<ControllerDispatcher, 1> instance_limit;
	static inline ControllerDispatcher* instance;
	Logger::Category cat;

	struct Controller {
		ControllerID id;
		string name;
		vector<bool> buttons;
		vector<float> axes;
	};
	array<Controller, 16> controllers{};

	static void joystick_event_callback(int jid, int event);
};

}
