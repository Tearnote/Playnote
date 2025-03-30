#pragma once

#include <algorithm>
#include <ranges>
#include <span>
#include "util/types.hpp"

using std::ranges::transform;
using std::ranges::views::iota;
using std::ranges::views::reverse;

// Safely create a span from pointer+size pair
// Creates a valid empty span if size is 0 or pointer is null
template<typename T>
auto ptr_span(T* ptr, usize size = 1)
{
	if (!ptr || !size)
		return std::span<T>();
	return std::span(ptr, size);
}
