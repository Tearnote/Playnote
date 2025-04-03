#pragma once

#include "sys/window.hpp"

class EventLoop {
public:
	void process()
	{
		while (!s_window->isClosing())
			s_window->poll();
	}
};
