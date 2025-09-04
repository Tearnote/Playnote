/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/utility.hpp:
Imports and helpers for generic utilities.
*/

#pragma once
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
#include <bit>
#include <boost/scope/unique_resource.hpp>
#define MAGIC_ENUM_RANGE_MIN -1
#define MAGIC_ENUM_RANGE_MAX 64
#include <magic_enum/magic_enum.hpp>
#include "preamble/types.hpp"

namespace playnote {

using std::optional;
using std::nullopt;
using std::make_optional;
using std::variant;
using std::monostate;
using std::holds_alternative;
using std::get;
using std::visit;
using std::move;
using std::forward;
using std::pair;
using std::make_pair;
using std::tuple;
using std::make_tuple;
using std::piecewise_construct;
using std::reference_wrapper;
using std::ref;
using std::initializer_list;
using std::unique_ptr;
using std::make_unique;
using std::shared_ptr;
using std::make_shared;
using std::static_pointer_cast;
using std::enable_shared_from_this;
using std::weak_ptr;
using boost::scope::unique_resource;
using std::type_index;
using std::remove_cvref_t;
using std::bit_cast;
using magic_enum::enum_name;
using magic_enum::enum_cast;
using magic_enum::enum_count;

// Constructs a type with overloaded operator()s, for use as a std::variant visitor
template<typename... Ts>
struct visitor: Ts... {
	using Ts::operator()...;
};

template<typename T>
concept scoped_enum = std::is_scoped_enum_v<T>;

// Convenient shorthand for getting the value of an enum class
constexpr auto operator+(scoped_enum auto val) noexcept { return std::to_underlying(val); }

// Add as class member to limit the number of simultaneous instances.
// Throws logic_error if the limit is reached.
template<typename, usize Limit>
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
