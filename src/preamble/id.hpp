/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include "preamble/string.hpp"
#include "preamble/types.hpp"

namespace playnote {

// A hash value generated from a string, possibly at compile-time.
class id {
public:
	id() = default;

	explicit constexpr id(string_view str): val{Basis}
	{
		for (auto const ch: str) {
			val ^= ch;
			val *= Prime;
		}
	}

	constexpr auto operator<=>(id const&) const = default;
	constexpr auto operator+() const { return val; }

private:
	static constexpr auto Prime = 16777619u;
	static constexpr auto Basis = 2166136261u;

	uint val;
};

// Generate the hash of a string at compile-time.
[[nodiscard]] consteval auto operator ""_id(char const* str, size_t len) -> id {
	return id{string_view{str, len}};
}

// boost::hash implementation for id.
[[nodiscard]] constexpr auto hash_value(id const& v) -> uint { return +v; }

}
