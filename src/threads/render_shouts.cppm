/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render_shouts.cppm:
Shouts that can be spawned by the render thread.
*/

export module playnote.threads.render_shouts;

namespace playnote::threads {

// User requested an action on the playing chart.
export enum class PlayerControl {
	Play,
	Pause,
	Restart,
};

}
