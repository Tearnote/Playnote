/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports and helpers for generic utilities.
*/

module;
#include <initializer_list>
#include <optional>
#include <variant>
#include <utility>
#include <tuple>

export module playnote.preamble:utility;

namespace playnote {

export using std::optional;
export using std::variant;
export using std::move;
export using std::forward;
export using std::pair;
export using std::tuple;
export using std::initializer_list;

// Constructs a type with overloaded operator()s, for use as a std::variant visitor
export template<typename... Ts>
struct visitor: Ts... {
	using Ts::operator()...;
};

}
