/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/mpmc.hpp:
Wrapper for concurrentqueue.
*/

#pragma once
#include "concurrentqueue.h"
#include "preamble.hpp"

namespace playnote::lib::mpmc {

template<typename T>
using Queue = moodycamel::ConcurrentQueue<T>;

}
