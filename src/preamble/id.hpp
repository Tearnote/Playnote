/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/id.hpp:
A compile-time-hashed string, mainly for use as a hashmap key.
*/

#pragma once
#include "preamble/string.hpp"
#include "preamble/types.hpp"

namespace playnote {

// A hash value generated from a string, possibly at compile-time.
class id {
public:
	id() = default;

	explicit constexpr id(string_view str): val{Basis} {
		for (auto const ch: str) {
			val ^= ch;
			val *= Prime;
		}
	}

	constexpr auto operator==(id const&) const -> bool = default;
	constexpr auto operator!=(id const&) const -> bool = default;
	constexpr auto operator+() const { return val; }

private:
	static constexpr auto Prime = 16777619u;
	static constexpr auto Basis = 2166136261u;

	uint32 val;
};

// Generate the hash of a string at compile-time.
[[nodiscard]] consteval auto operator ""_id(char const* str, usize len) -> id {
	return id{string_view{str, len}};
}

// boost::hash implementation for id.
[[nodiscard]] constexpr auto hash_value(id const& v) -> isize { return +v; }

}
