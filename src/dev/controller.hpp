/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

dev/controller.hpp:
Handling of controller input devices.
*/

#pragma once

#include <GLFW/glfw3.h>
#include "preamble.hpp"
#include "logger.hpp"

namespace playnote::dev {

class ControllerDispatcher {
public:
	ControllerDispatcher();

	template<callable<void()> Func>
	void poll(Func&&);

private:
	InstanceLimit<ControllerDispatcher, 1> instance_limit;
	static inline ControllerDispatcher* instance;

	struct Controller {
		id guid; // Hash of the GUID
		uint32 duplicate_id; // Initially 0, incremented if a duplicate GUID is found
		vector<bool> buttons;
		vector<float> axes;
	};
	array<Controller, GLFW_JOYSTICK_LAST + 1> controllers{};

	static void joystick_event_callback(int id, int event);
};

inline ControllerDispatcher::ControllerDispatcher()
{
	instance = this;
	for (auto jid: views::iota(GLFW_JOYSTICK_1, GLFW_JOYSTICK_LAST + 1))
		if (glfwJoystickPresent(jid)) joystick_event_callback(jid, GLFW_CONNECTED);
	glfwSetJoystickCallback(joystick_event_callback);
}

template<callable<void()> Func>
void ControllerDispatcher::poll(Func&& func)
{
	//TODO
}

inline void ControllerDispatcher::joystick_event_callback(int jid, int event)
{
	if (event == GLFW_DISCONNECTED) return;
	auto& self = *instance;
	auto& controller = self.controllers[jid];
	auto guid = id{glfwGetJoystickGUID(jid)};

	auto dids_used = array<bool, GLFW_JOYSTICK_LAST + 1>{};
	for (auto& c: self.controllers) {
		if (c.guid == guid) dids_used[c.duplicate_id] = true;
	}
	auto lowest_unused_did = distance(dids_used.begin(), find(dids_used, false));
	controller.guid = guid;
	controller.duplicate_id = lowest_unused_did;

	auto button_count = 0;
	auto const* buttons = glfwGetJoystickButtons(jid, &button_count);
	controller.buttons.clear();
	controller.buttons.reserve(button_count);
	transform(span{buttons, static_cast<uint32>(button_count)}, back_inserter(controller.buttons), [](auto state) {
		return state == GLFW_PRESS;
	});

	auto axes_count = 0;
	auto const* axes = glfwGetJoystickAxes(jid, &axes_count);
	controller.axes.clear();
	controller.axes.reserve(axes_count);
	transform(span{axes, static_cast<uint32>(axes_count)}, back_inserter(controller.axes), [](auto value) {
		return value;
	});

	INFO("Controller connected: \"{}\", ID: {};{}", glfwGetJoystickName(jid), glfwGetJoystickGUID(jid), lowest_unused_did);
}

}
