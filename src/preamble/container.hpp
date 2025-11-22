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
#include <iterator>
#include <string>
#include <array>
#include <span>
#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/vector.hpp>
#include "readerwriterqueue.h"
#include "concurrentqueue.h"
#include "plf_colony.h"
#include "preamble/types.hpp"

namespace playnote {

using boost::container::vector;
using boost::container::static_vector;
using boost::container::small_vector;
using plf::colony;
using std::back_inserter;
using std::array;
using std::to_array;
template<typename Key, typename T, typename Hash = boost::hash<Key>>
using unordered_map = boost::unordered_flat_map<Key, T, Hash, std::equal_to<>>;
template<typename Key, typename Hash = boost::hash<Key>>
using unordered_set = boost::unordered_flat_set<Key, Hash, std::equal_to<>>;
using boost::unordered_node_map;
using std::span;
namespace pmr {
	using boost::container::pmr::vector;
	using boost::container::pmr::monotonic_buffer_resource;
	using boost::container::pmr::polymorphic_allocator;
}

// Lock-free containers
template<typename T>
using spsc_queue = moodycamel::ReaderWriterQueue<T>;
template<typename T>
using mpmc_queue = moodycamel::ConcurrentQueue<T>;

// Custom hash function that enables heterogenous lookup.
// https://www.cppstories.com/2021/heterogeneous-access-cpp20/
struct string_hash {
	using is_transparent = void;
	[[nodiscard]] auto operator()(char const* text) const -> size_t {
		return boost::hash<std::string_view>{}(text);
	}
	[[nodiscard]] auto operator()(std::string_view text) const -> size_t {
		return boost::hash<std::string_view>{}(text);
	}
	[[nodiscard]] auto operator()(std::string const& text) const -> size_t {
		return boost::hash<std::string>{}(text);
	}
};

}
