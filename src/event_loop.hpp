#pragma once

#include "sys/window.hpp"

class EventLoop:
	Window
{
public:
	void process()
	{
		while (!Window::serv->isClosing())
			Window::serv->poll();
	}
};
