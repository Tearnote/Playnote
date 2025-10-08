/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

lib/mpmc.hpp:
A multi-producer, multi-consumer lock-free queue.
*/

#pragma once
#include "preamble.hpp"
#include "concurrentqueue.h"

namespace playnote::lib::mpmc {

template<typename T>
using Queue = moodycamel::ConcurrentQueue<T>;

}
