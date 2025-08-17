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
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/irange.hpp>

namespace playnote {

using std::ranges::contains;
using std::ranges::fill;
using std::ranges::copy;
using std::ranges::transform;
using std::ranges::find_if;
using std::ranges::find_last_if;
using std::ranges::sort;
using std::ranges::stable_sort;
using std::ranges::unique;
using std::ranges::reverse;
using std::ranges::remove_if;
using std::ranges::fold_left;
using std::ranges::for_each;
using std::ranges::any_of;

using std::function;
using std::invoke;
using boost::irange;
using boost::adaptors::reversed;

}
