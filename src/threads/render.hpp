/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.hpp:
Presents current game state onto the window at the screen's refresh rate.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "threads/broadcaster.hpp"

namespace playnote::threads {

// Render thread entry point.
void render(Broadcaster& broadcaster, Barriers<3>& barriers, dev::Window& window);

}
