/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once

#include <GLFW/glfw3.h>
#include "preamble.hpp"
#include "utils/assert.hpp"
#include "utils/logger.hpp"
#include "lib/glfw.hpp"
#include "dev/window.hpp"
#include "input.hpp"

namespace playnote::dev {

class ControllerDispatcher {
public:
	explicit ControllerDispatcher(Logger::Category);

	using ControllerEvent = variant<ButtonInput, AxisInput>;
	template<callable<void(ControllerEvent)> Func>
	void poll(Func&&);

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
	array<Controller, GLFW_JOYSTICK_LAST + 1> controllers{};

	static void joystick_event_callback(int jid, int event);
};

inline ControllerDispatcher::ControllerDispatcher(Logger::Category cat):
	cat{cat}
{
	instance = this;
	for (auto jid: views::iota(GLFW_JOYSTICK_1, GLFW_JOYSTICK_LAST + 1))
		if (glfwJoystickPresent(jid)) joystick_event_callback(jid, GLFW_CONNECTED);
	glfwSetJoystickCallback(joystick_event_callback);
}

template<callable<void(ControllerDispatcher::ControllerEvent)> Func>
void ControllerDispatcher::poll(Func&& func)
{
	for (auto jid: views::iota(GLFW_JOYSTICK_1, GLFW_JOYSTICK_LAST + 1)) {
		if (!glfwJoystickPresent(jid)) continue;
		auto& controller = controllers[jid];

		auto button_count = 0;
		auto const* buttons_ptr = glfwGetJoystickButtons(jid, &button_count);
		auto buttons = span{buttons_ptr, static_cast<size_t>(button_count)};
		for (auto [idx, previous, current_raw]: views::zip(views::iota(0), controller.buttons, buttons)) {
			auto current = current_raw == +lib::glfw::Action::Press;
			if (previous == current) continue;
			func(ControllerEvent{ButtonInput{
				.controller = controller.id,
				.timestamp = globals::glfw->get_time(),
				.button = idx,
				.state = current,
			}});
			previous = current;
		}

		auto axes_count = 0;
		auto const* axes_ptr = glfwGetJoystickAxes(jid, &axes_count);
		auto axes = span{axes_ptr, static_cast<size_t>(axes_count)};
		for (auto [idx, previous, current]: views::zip(views::iota(0), controller.axes, axes)) {
			if (previous == current) continue;
			func(ControllerEvent{AxisInput{
				.controller = controller.id,
				.timestamp = globals::glfw->get_time(),
				.axis = idx,
				.value = current,
			}});
			previous = current;
		}
	}
}

inline void ControllerDispatcher::joystick_event_callback(int jid, int event)
{
	auto& self = *instance;
	auto& controller = self.controllers[jid];
	if (event == GLFW_DISCONNECTED) {
		INFO_AS(self.cat, "Controller disconnected: \"{}\"", controller.name);
		controller.name.clear();
		controller.id = {};
		controller.buttons.clear();
		controller.axes.clear();
		return;
	}

	controller.name = glfwGetJoystickName(jid);
	auto guid = id{glfwGetJoystickGUID(jid)};

	auto dids_used = array<bool, GLFW_JOYSTICK_LAST + 1>{};
	for (auto& c: self.controllers) {
		if (c.id.guid == guid) dids_used[c.id.duplicate] = true;
	}
	auto lowest_unused_did = distance(dids_used.begin(), find(dids_used, false));
	controller.id.guid = guid;
	controller.id.duplicate = lowest_unused_did;

	auto button_count = 0;
	auto const* buttons_ptr = glfwGetJoystickButtons(jid, &button_count);
	auto buttons = span{buttons_ptr, static_cast<size_t>(button_count)};
	ASSERT(controller.buttons.empty());
	controller.buttons.reserve(button_count);
	transform(buttons, back_inserter(controller.buttons), [](auto state) {
		return state == +lib::glfw::Action::Press;
	});

	auto axes_count = 0;
	auto const* axes_ptr = glfwGetJoystickAxes(jid, &axes_count);
	auto axes = span{axes_ptr, static_cast<size_t>(axes_count)};
	ASSERT(controller.axes.empty());
	controller.axes.reserve(axes_count);
	transform(axes, back_inserter(controller.axes), [](auto value) {
		return value;
	});

	INFO_AS(self.cat, "Controller connected: \"{}\", ID: {};{}", glfwGetJoystickName(jid), glfwGetJoystickGUID(jid), lowest_unused_did);
}

}
