/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/tracy.hpp:
Wrapper for Tracy profiler functions.
*/

#pragma once
#include "tracy/Tracy.hpp"
#include "preamble.hpp"

namespace playnote::lib::tracy {

// Use at the end of a frame of rendering to implement frametime tracking.
#define FRAME_MARK() FrameMark

// Use at the end of secondary frames.
#define FRAME_MARK_NAMED(name) FrameMarkNamed(name)

// Set thread name as it's going to appear in the Tracy client.
inline void name_current_thread(string_view name)
{
	::tracy::SetThreadName(string{name}.c_str());
}

}
