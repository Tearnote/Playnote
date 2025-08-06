/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/container.hpp:
Imports and helpers for container types.
*/

#pragma once
#include <string_view>
#include <iterator>
#include <string>
#include <array>
#include <span>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/vector.hpp>
#include <boost/container/vector.hpp>
#include "preamble/types.hpp"

namespace playnote {

using boost::container::vector;
using boost::container::static_vector;
using boost::container::small_vector;
using std::back_inserter;
using std::array;
using std::to_array;
template<typename Key, typename T, typename Hash = boost::hash<Key>>
using unordered_map = boost::unordered_flat_map<Key, T, Hash, std::equal_to<>>;
using std::span;
namespace pmr {
	using boost::container::pmr::vector;
	using boost::container::pmr::monotonic_buffer_resource;
	using boost::container::pmr::polymorphic_allocator;
}

// Custom hash function that enables heterogenous lookup.
// https://www.cppstories.com/2021/heterogeneous-access-cpp20/
struct string_hash {
	using is_transparent = void;
	[[nodiscard]] usize operator()(char const* text) const {
		return boost::hash<std::string_view>{}(text);
	}
	[[nodiscard]] usize operator()(std::string_view text) const {
		return boost::hash<std::string_view>{}(text);
	}
	[[nodiscard]] usize operator()(std::string const& text) const {
		return boost::hash<std::string>{}(text);
	}
};

}
