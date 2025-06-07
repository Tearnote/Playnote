/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.cppm:
Imports and helpers for generic utilities.
*/

module;
#include <initializer_list>
#include <type_traits>
#include <functional>
#include <stdexcept>
#include <typeindex>
#include <optional>
#include <variant>
#include <utility>
#include <memory>
#include <tuple>
#include <boost/scope/unique_resource.hpp>

export module playnote.preamble:utility;

import :types;

namespace playnote {

export using std::optional;
export using std::nullopt;
export using std::variant;
export using std::monostate;
export using std::holds_alternative;
export using std::get;
export using std::visit;
export using std::move;
export using std::forward;
export using std::pair;
export using std::make_pair;
export using std::tuple;
export using std::make_tuple;
export using std::piecewise_construct;
export using std::reference_wrapper;
export using std::ref;
export using std::initializer_list;
export using std::unique_ptr;
export using std::make_unique;
export using std::shared_ptr;
export using std::make_shared;
export using std::static_pointer_cast;
export using boost::scope::unique_resource;
export using std::type_index;
export using std::remove_cvref_t;

// Constructs a type with overloaded operator()s, for use as a std::variant visitor
export template<typename... Ts>
struct visitor: Ts... {
	using Ts::operator()...;
};

export template<typename T>
concept scoped_enum = std::is_scoped_enum_v<T>;

// Convenient shorthand for getting the value of an enum class
export constexpr auto operator+(scoped_enum auto val) noexcept { return std::to_underlying(val); }

// Add as class member to limit the number of simultaneous instances.
// Throws logic_error if the limit is reached.
export template<typename, usize Limit>
class InstanceLimit {
public:
	InstanceLimit()
	{
		count += 1;
		if (count > Limit) throw std::logic_error{"Instance limit reached"};
	}

	~InstanceLimit() noexcept { count -= 1; }

	// Default copy and move constructors should behave as expected

private:
	static inline auto count = 0zu;
};

}
