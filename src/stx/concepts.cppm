/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

stx/concepts.cppm:
Useful generic concepts.
*/

module;
#include <type_traits>
#include <variant>

export module playnote.stx.concepts;

namespace playnote::stx {

template<class T, class Variant>
inline constexpr auto is_variant_alternative_v = false;

template<class T, class... Ts>
inline constexpr auto is_variant_alternative_v<T, std::variant<Ts...>> =
	(... or std::is_same_v<T, Ts>);

// Constrain type T to one of a std::variant's available alternatives
export template<class T, class Variant>
concept is_variant_alternative = is_variant_alternative_v<T, Variant>;

}
