/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.hpp:
A thread that manages game state and presents it into the window.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "utils/broadcaster.hpp"

namespace playnote {

// Render thread entry point.
void render_thread(Broadcaster& broadcaster, Barriers<2>& barriers, dev::Window& window);

}
