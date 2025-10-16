/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/render.hpp:
A thread that manages game state and presents it into the window.
*/

#pragma once
#include "preamble.hpp"
#include "dev/window.hpp"
#include "threads/tools.hpp"

namespace playnote::threads {

// Render thread entry point.
void render(Tools& tools, dev::Window& window);

}
