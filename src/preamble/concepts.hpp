/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <type_traits>
#include <concepts>
#include "preamble/utility.hpp"

namespace playnote {

using std::same_as;
using std::convertible_to;

template<typename T, typename Base>
concept implements = std::derived_from<T, Base>;

template<class T, class Variant>
inline constexpr auto is_variant_alternative_v = false;

template<class T, class... Ts>
inline constexpr auto is_variant_alternative_v<T, variant<Ts...>> =
	(... || std::is_same_v<T, Ts>);

// Constrain type T to one of a std::variant's available alternatives
template<class T, class Variant>
concept variant_alternative = is_variant_alternative_v<T, Variant>;

// "applicable" concept
// Analogous to std::invocable but accepts a tuple rather than argument pack
template<typename, typename>
struct is_applicable: std::false_type {};

template<typename Func, template<typename...> typename Tuple, typename... Args>
struct is_applicable<Func, Tuple<Args...>>: std::is_invocable<Func, Args...> {};

template<typename F, typename Tuple>
concept applicable = is_applicable<F, Tuple>::value;

// "apply_results_in" concept
// Analogous to std::invoke_result type compatibility check but accepts a tuple rather than argument pack
template<typename, typename>
struct apply_result: std::false_type {};

template<typename Func, template<typename...> typename Tuple, typename... Args>
struct apply_result<Func, Tuple<Args...>>: std::invoke_result<Func, Args...> {};

template<typename F, typename Rtype, typename Tuple>
concept apply_results_in = std::convertible_to<typename apply_result<F, Tuple>::type, Rtype>;

// "callable_unpack" helper struct
// Provides return type and tuple of arguments from a function type expression
template<typename>
struct callable_unpack;

template<typename R, typename... Args>
struct callable_unpack<R(Args...)> {
	using args = tuple<Args...>;
	using rtype = R;
};

template<class F, typename Sig,
	typename Unpack = callable_unpack<Sig>,
	typename ArgsT = typename Unpack::args,
	typename Rtype = typename Unpack::rtype>
concept callable = applicable<F, ArgsT> && apply_results_in<F, Rtype, ArgsT>;

}
