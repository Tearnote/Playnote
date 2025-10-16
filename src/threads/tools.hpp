/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

threads/tools.hpp:
A package of communication and task processing utilities for threads to utilize.
*/

#pragma once
#include "preamble.hpp"
#include "utils/broadcaster.hpp"

namespace playnote::threads {

struct Tools {
	Broadcaster broadcaster;
	Barriers<2> barriers;
};

}
