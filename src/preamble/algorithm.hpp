/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/algorithm.hpp:
Imports and helpers for STL algorithm support.
*/

#pragma once
#include <string_view>
#include <functional>
#include <algorithm>
#include <ranges>

namespace playnote {

namespace views {
	using std::ranges::views::iota;
	using std::ranges::views::zip;
	using std::ranges::views::split;
	using std::ranges::views::chunk;
	using std::ranges::views::filter;
	using std::ranges::views::enumerate;
	using std::ranges::views::values;
	using std::ranges::views::pairwise;
	using std::ranges::views::take;
	using std::ranges::views::reverse;
	using std::ranges::views::transform;
	using std::ranges::views::drop_while;
	using std::ranges::views::take_while;

	// Helper view for converting a string subrange to a string_view
	inline auto to_sv = transform([](auto range) {
		return std::string_view{range};
	});
}
using std::ranges::contains;
using std::ranges::fill;
using std::ranges::copy;
using std::ranges::transform;
using std::ranges::find_if;
using std::ranges::sort;
using std::ranges::stable_sort;
using std::ranges::unique;
using std::ranges::reverse;
using std::ranges::remove_if;
using std::ranges::fold_left;
using std::ranges::for_each;

using std::function;
using std::invoke;

}
