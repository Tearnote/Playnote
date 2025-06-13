/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/algorithm.cppm:
Imports and helpers for STL algorithm support.
*/

module;
#include <string_view>
#include <functional>
#include <algorithm>
#include <vector>
#include <ranges>

export module playnote.preamble:algorithm;

namespace playnote {

namespace views {
	export using std::ranges::views::iota;
	export using std::ranges::views::zip;
	export using std::ranges::views::split;
	export using std::ranges::views::chunk;
	export using std::ranges::views::filter;
	export using std::ranges::views::enumerate;
	export using std::ranges::views::values;
	export using std::ranges::views::pairwise;

	// Helper view for converting a string subrange to a string_view
	export inline auto to_sv = std::ranges::views::transform([](auto range) {
		return std::string_view{range};
	});
}
export using std::ranges::contains;
export using std::ranges::fill;
export using std::ranges::copy;
export using std::ranges::transform;
export using std::ranges::find_if;
export using std::ranges::sort;
export using std::ranges::stable_sort;
export using std::ranges::unique;
export using std::ranges::reverse;
export using std::ranges::remove_if;
export using std::ranges::fold_left;
export using std::ranges::views::__adaptor::operator|; // Non-portable, but can't do anything else without header units

export using std::function;
export using std::invoke;

}
