/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/>. This file may not be copied, modified, or distributed
except according to those terms.
*/

#pragma once
#include "dev/window.hpp"
#include "utils/broadcaster.hpp"

namespace playnote {

// Render thread entry point.
void render_thread(Broadcaster& broadcaster, Barriers<2>& barriers, dev::Window& window);

}
