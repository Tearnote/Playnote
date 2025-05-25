/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

macros/tracing.hpp:
Macro wrappers for the Tracy profiler.
*/

#ifndef PLAYNOTE_MACROS_TRACING_H
#define PLAYNOTE_MACROS_TRACING_H

#include "tracy/Tracy.hpp"

// Use at the end of a frame of rendering to implement frametime tracking.
#define FRAME_MARK() FrameMark

// Use at the end of secondary frames.
#define FRAME_MARK_NAMED(name) FrameMarkNamed(name)

#endif
