/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/algorithm.cppm:
Imports and helpers for STL algorithm support.
*/

module;
#include <functional>
#include <algorithm>
#include <ranges>

export module playnote.preamble:algorithm;

namespace playnote {

namespace views {
	export using std::ranges::views::iota;
	export using std::ranges::views::zip;
}
export using std::ranges::contains;
export using std::ranges::copy;
export using std::ranges::transform;

export using std::function;
export using std::invoke;

}
