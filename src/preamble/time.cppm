/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/os.cppm:
Imports of clock/time/date functionality.
*/

module;
#include <chrono>

export module playnote.preamble:time;

namespace playnote {

export using std::literals::operator ""ns;
export using std::literals::operator ""ms;
export using std::literals::operator ""s;
export using std::chrono::nanoseconds;
export using std::chrono::milliseconds;
export using std::chrono::seconds;
export using std::chrono::duration;
export using std::chrono::duration_cast;

}
