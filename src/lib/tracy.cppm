/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/tracy.cppm:
Wrapper for Tracy profiler functions.
*/

module;
#include "tracy/Tracy.hpp"
#include "preamble.hpp"

export module playnote.lib.tracy;

namespace playnote::lib::tracy {

// Set thread name as it's going to appear in the Tracy client.
export void name_current_thread(string_view name)
{
	::tracy::SetThreadName(string{name}.c_str());
}

}
