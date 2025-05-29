/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports and helpers for container types.
*/

module;
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

export module playnote.preamble:container;

import :types;

namespace playnote {

export using boost::container::vector;
export using boost::container::static_vector;
export using boost::container::small_vector;
export using std::back_inserter;
export using std::array;
export using std::to_array;
export template<typename Key, typename T, typename Hash = boost::hash<Key>>
using unordered_map = boost::unordered_flat_map<Key, T, Hash, std::equal_to<>>;
export using std::span;
namespace pmr {
	export using boost::container::pmr::vector;
	export using boost::container::pmr::monotonic_buffer_resource;
	export using boost::container::pmr::polymorphic_allocator;
}

// Custom hash function that enables heterogenous lookup.
// https://www.cppstories.com/2021/heterogeneous-access-cpp20/
export struct string_hash {
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
