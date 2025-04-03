#ifndef PLAYNOTE_EVENT_LOOP_H
#define PLAYNOTE_EVENT_LOOP_H

#include "sys/window.hpp"

class EventLoop {
public:
	void process()
	{
		while (!s_window->isClosing())
			s_window->poll();
	}
};

#endif
