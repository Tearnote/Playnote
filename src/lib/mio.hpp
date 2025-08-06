/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/mio.hpp:
Wrapper for mio file mapping.
*/

#pragma once
#include "mio/mmap.hpp"
#include "preamble.hpp"

namespace playnote::lib::mio {

using ReadMapping = ::mio::basic_mmap_source<byte>;

}
