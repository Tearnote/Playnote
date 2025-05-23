/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports and helpers for container types.
*/

module;
#include <iterator>
#include <vector>
#include <array>
#include <span>
#include <boost/unordered/unordered_flat_map.hpp>

export module playnote.preamble:container;

namespace playnote {

export using std::vector;
export using __gnu_cxx::operator==; // temporary hack
export using std::back_inserter;
export using std::array;
export using std::to_array;
export template<typename Key, typename T, typename Hash = boost::hash<Key>>
using unordered_map = boost::unordered_flat_map<Key, T, Hash>;
export using std::span;
export using std::begin;
export using std::end;
namespace pmr {
	export using std::pmr::vector;
	export using std::pmr::monotonic_buffer_resource;
	export using std::pmr::polymorphic_allocator;
}

}
