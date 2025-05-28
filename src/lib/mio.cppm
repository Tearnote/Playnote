/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/mio.cppm:
Wrapper for mio file mapping.
*/

module;
#include "mio/mmap.hpp"

export module playnote.lib.mio;

import playnote.preamble;

namespace playnote::lib::mio {

export using ReadMapping = ::mio::basic_mmap_source<byte>;

}
