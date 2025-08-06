/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/time.hpp:
Imports of clock/time/date functionality.
*/

#pragma once
#include <chrono>

namespace playnote {

using std::literals::operator ""ns;
using std::literals::operator ""ms;
using std::literals::operator ""s;
using std::chrono::nanoseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::ratio;

}
