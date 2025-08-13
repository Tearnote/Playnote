/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio.hpp:
Initializes audio and handles queueing of playback events.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

// Audio thread entry point.
void audio(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window);

}
