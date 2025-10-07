/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/audio_events.hpp:
Shouts that can be spawned by the audio thread.
*/

#pragma once
#include "preamble.hpp"
#include "audio/player.hpp"

namespace playnote::threads {

struct ChartLoaded {
	weak_ptr<audio::Player const> player;
};

}
