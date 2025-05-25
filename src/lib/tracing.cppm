/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/tracing.cppm:
Wrapper for Tracy profiler functions.
*/

module;
#include "tracy/Tracy.hpp"

export module playnote.lib.tracing;

import playnote.preamble;

namespace playnote::lib {

// Set thread name as it's going to appear in the Tracy client.
export void tracing_set_thread_name(string_view name) noexcept
{
	tracy::SetThreadName(string{name}.c_str());
}

}
