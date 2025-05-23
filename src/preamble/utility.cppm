/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports and helpers for generic utilities.
*/

module;
#include <initializer_list>
#include <functional>
#include <optional>
#include <variant>
#include <utility>
#include <memory>
#include <tuple>

export module playnote.preamble:utility;

namespace playnote {

export using std::optional;
export using std::nullopt;
export using std::variant;
export using std::monostate;
export using std::holds_alternative;
export using std::get;
export using std::move;
export using std::forward;
export using std::pair;
export using std::make_pair;
export using std::tuple;
export using std::make_tuple;
export using std::ref;
export using std::initializer_list;
export using std::unique_ptr;
export using std::make_unique;
export using std::shared_ptr;
export using std::make_shared;
export using std::static_pointer_cast;
export using std::to_underlying;

// Constructs a type with overloaded operator()s, for use as a std::variant visitor
export template<typename... Ts>
struct visitor: Ts... {
	using Ts::operator()...;
};

}
