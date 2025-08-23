/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render_shouts.hpp:
Shouts that can be spawned by the render thread.
*/

#pragma once

namespace playnote::threads {

// User requested an action on the playing chart.
enum class PlayerControl {
	Play,
	Pause,
	Restart,
	Autoplay,
};

}
