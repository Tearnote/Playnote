/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
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
	using std::ranges::views::chunk_by;
	using std::ranges::views::filter;
	using std::ranges::views::enumerate;
	using std::ranges::views::keys;
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
using std::ranges::all_of;
using std::ranges::any_of;
using std::ranges::contains;
using std::ranges::fill;
using std::ranges::copy;
using std::ranges::transform;
using std::ranges::find;
using std::ranges::find_if;
using std::ranges::find_last_if;
using std::ranges::sort;
using std::ranges::stable_sort;
using std::ranges::unique;
using std::ranges::reverse;
using std::ranges::remove_if;
using std::ranges::fold_left;
using std::ranges::any_of;
using std::ranges::min_element;
using std::ranges::max_element;
using std::ranges::elements_of;

using std::distance;
using std::function;

}
