/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render_shouts.hpp:
Shouts that can be spawned by the render thread.
*/

#pragma once

#include "preamble.hpp"
#include "lib/openssl.hpp"

namespace playnote::threads {

// Request for the current contents of the library.
struct LibraryRefreshRequest {};

// User wants to play a chart.
using LoadChart = lib::openssl::MD5;

// User requested an action on the playing chart.
enum class PlayerControl {
	Play,
	Pause,
	Restart,
	Autoplay,
	Stop,
};

}
