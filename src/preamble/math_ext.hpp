/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/math_ext.hpp:
Additional math types and functions. Introduces 2-4 component vectors.
*/

#pragma once
#include <type_traits>
#include <concepts>
#include <numbers>
#include "preamble/algorithm.hpp"
#include "preamble/container.hpp"
#include "preamble/types.hpp"
#include "preamble/math.hpp"

namespace playnote {

// A built-in type with defined arithmetic operations (+, -, *, /)
template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

// https://www.tauday.com/
template<std::floating_point T>
constexpr auto Tau_v = std::numbers::pi_v<T> * 2;
constexpr auto Tau = Tau_v<float>;

// degrees -> radians
template<arithmetic T, std::floating_point Prec = float>
constexpr auto radians(T deg) -> Prec { return static_cast<Prec>(deg) * Tau_v<Prec> / 360; }

// True modulo operation (as opposed to remainder, which is operator% in C++.)
// The result is always positive and does not flip direction at zero.
// Example:
//  5 mod 4 = 1
//  4 mod 4 = 0
//  3 mod 4 = 3
//  2 mod 4 = 2
//  1 mod 4 = 1
//  0 mod 4 = 0
// -1 mod 4 = 3
// -2 mod 4 = 2
// -3 mod 4 = 1
// -4 mod 4 = 0
// -5 mod 4 = 3
template<std::integral T>
[[nodiscard]] constexpr auto tmod(T num, T div) { return num % div + (num % div < 0) * div; }

// GLSL-style scalar clamp
template<arithmetic T>
[[nodiscard]] constexpr auto clamp(T val, T vmin, T vmax) -> T
{
	return max(vmin, min(val, vmax));
}

// Generic math vector, of any dimension between 2 to 4 and any underlying type
template<usize Dim, arithmetic T>
class vec {
public:
	static_assert(Dim >= 2 && Dim <= 4, "Vectors need to have 2, 3 or 4 components");
	using self_t = vec;

	// Uninitialized init
	constexpr vec() = default;
	// Fill the vector with copies of the value
	explicit constexpr vec(T fillVal) { fill(fillVal); }
	// Create the vector with provided component values
	constexpr vec(initializer_list<T> list) { copy(list, arr.begin()); }

	// Type cast
	template<arithmetic U>
		requires (!std::same_as<T, U>)
	explicit constexpr vec(vec<Dim, U> const& other);

	// Dimension downcast
	template<usize N>
		requires (N > Dim)
	explicit constexpr vec(vec<N, T> const& other);

	// Dimension upcast
	template<usize N>
		requires (N < Dim)
	constexpr vec(vec<N, T> const& other, T fill);

	[[nodiscard]] constexpr auto at(usize n) -> T& { return arr[n]; }
	[[nodiscard]] constexpr auto at(usize n) const -> T { return arr[n]; }

	[[nodiscard]] constexpr auto operator[](usize n) -> T& { return at(n); }
	[[nodiscard]] constexpr auto operator[](usize n) const -> T { return at(n); }

	[[nodiscard]] constexpr auto x() -> T&;
	[[nodiscard]] constexpr auto x() const -> T;
	[[nodiscard]] constexpr auto y() -> T&;
	[[nodiscard]] constexpr auto y() const -> T;
	[[nodiscard]] constexpr auto z() -> T&;
	[[nodiscard]] constexpr auto z() const -> T;
	[[nodiscard]] constexpr auto w() -> T&;
	[[nodiscard]] constexpr auto w() const -> T;

	[[nodiscard]] constexpr auto r() -> T& { return x(); }
	[[nodiscard]] constexpr auto r() const -> T { return x(); }
	[[nodiscard]] constexpr auto g() -> T& { return y(); }
	[[nodiscard]] constexpr auto g() const -> T { return y(); }
	[[nodiscard]] constexpr auto b() -> T& { return z(); }
	[[nodiscard]] constexpr auto b() const -> T { return z(); }
	[[nodiscard]] constexpr auto a() -> T& { return w(); }
	[[nodiscard]] constexpr auto a() const -> T { return w(); }

	constexpr void fill(T val) { arr.fill(val); }

	constexpr auto operator+=(self_t const& other) -> self_t&;
	constexpr auto operator-=(self_t const& other) -> self_t&;
	constexpr auto operator*=(self_t const& other) -> self_t&;
	constexpr auto operator/=(self_t const& other) -> self_t&;
	constexpr auto operator%=(self_t const& other) -> self_t&;

	constexpr auto operator*=(T other) -> self_t&;
	constexpr auto operator/=(T other) -> self_t&;
	constexpr auto operator%=(T other) -> self_t&;
	constexpr auto operator<<=(T other) -> self_t&;
	constexpr auto operator>>=(T other) -> self_t&;

private:
	array<T, Dim> arr;
};

template<usize Dim, arithmetic T>
template<arithmetic U>
	requires (!same_as<T, U>)
constexpr vec<Dim, T>::vec(vec<Dim, U> const& other)
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] = static_cast<T>(other[i]);
}

template<usize Dim, arithmetic T>
template<usize N>
	requires (N > Dim)
constexpr vec<Dim, T>::vec(vec<N, T> const& other)
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] = other[i];
}

template<usize Dim, arithmetic T>
template<usize N>
	requires (N < Dim)
constexpr vec<Dim, T>::vec(vec<N, T> const& other, T fill)
{
	arr.fill(fill);
	for (auto i: views::iota(0uz, N))
		arr[i] = other[i];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::x() -> T&
{
	static_assert(Dim >= 1);
	return arr[0];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::x() const -> T
{
	static_assert(Dim >= 1);
	return arr[0];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::y() -> T&
{
	static_assert(Dim >= 2);
	return arr[1];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::y() const -> T
{
	static_assert(Dim >= 2);
	return arr[1];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::z() -> T&
{
	static_assert(Dim >= 3);
	return arr[2];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::z() const -> T
{
	static_assert(Dim >= 3);
	return arr[2];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::w() -> T&
{
	static_assert(Dim >= 4);
	return arr[3];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::w() const -> T
{
	static_assert(Dim >= 4);
	return arr[3];
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator+=(self_t const& other) -> self_t&
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] += other[i];
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator-=(self_t const& other) -> self_t&
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] -= other[i];
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator*=(self_t const& other) -> self_t&
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] *= other[i];
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator/=(self_t const& other) -> self_t&
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] /= other[i];
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator%=(self_t const& other) -> self_t&
{
	static_assert(std::is_integral_v<T>);

	for (auto i: views::iota(0uz, Dim))
		arr[i] %= other[i];
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator*=(T other) -> self_t&
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] *= other;
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator/=(T other) -> self_t&
{
	for (auto i: views::iota(0uz, Dim))
		arr[i] /= other;
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator%=(T other) -> self_t&
{
	static_assert(std::is_integral_v<T>);

	for (auto i: views::iota(0uz, Dim))
		arr[i] %= other;
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator<<=(T other) -> self_t&
{
	static_assert(std::is_integral_v<T>);

	for (auto i: views::iota(0uz, Dim))
		arr[i] <<= other;
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto vec<Dim, T>::operator>>=(T other) -> self_t&
{
	static_assert(std::is_integral_v<T>);

	for (auto i: views::iota(0uz, Dim))
		arr[i] >>= other;
	return *this;
}

template<usize Dim, arithmetic T>
constexpr auto operator+(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result += right;
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto operator-(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result -= right;
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto operator*(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result *= right;
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto operator/(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result /= right;
	return result;
}

template<usize Dim, std::integral T>
constexpr auto operator%(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result %= right;
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto operator==(vec<Dim, T> const& left, vec<Dim, T> const& right) -> bool
{
	for (auto i: views::iota(0uz, Dim)) {
		if (left[i] != right[i])
			return false;
	}
	return true;
}

template<usize Dim, arithmetic T>
constexpr auto min(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = vec<Dim, T>();
	for (auto i: views::iota(0uz, Dim))
		result[i] = min(left[i], right[i]);
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto max(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = vec<Dim, T>();
	for (auto i: views::iota(0uz, Dim))
		result[i] = max(left[i], right[i]);
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto operator*(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result *= right;
	return result;
}

template<usize Dim, arithmetic T>
constexpr auto operator*(T left, vec<Dim, T> const& right) -> vec<Dim, T> { return right * left; }

template<usize Dim, arithmetic T>
constexpr auto operator/(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result /= right;
	return result;
}

template<usize Dim, std::integral T>
constexpr auto operator%(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result %= right;
	return result;
}

template<usize Dim, std::integral T>
constexpr auto operator<<(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result <<= right;
	return result;
}

template<usize Dim, std::integral T>
constexpr auto operator>>(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result >>= right;
	return result;
}

// Component-wise absolute value
template<usize Dim, std::floating_point T>
constexpr auto abs(vec<Dim, T> const& v) -> vec<Dim, T>
{
	auto result = vec<Dim, T>{};
	for (auto i: views::iota(0uz, Dim))
		result[i] = abs(v[i]);
	return result;
}

// GLSL-like shorthands
using vec2 = vec<2, float>;
using vec3 = vec<3, float>;
using vec4 = vec<4, float>;
using ivec2 = vec<2, int32>;
using ivec3 = vec<3, int32>;
using ivec4 = vec<4, int32>;
using uvec2 = vec<2, uint32>;
using uvec3 = vec<3, uint32>;
using uvec4 = vec<4, uint32>;

static_assert(std::is_trivially_constructible_v<vec2>);
static_assert(std::is_trivially_constructible_v<vec3>);
static_assert(std::is_trivially_constructible_v<vec4>);
static_assert(std::is_trivially_constructible_v<ivec2>);
static_assert(std::is_trivially_constructible_v<ivec3>);
static_assert(std::is_trivially_constructible_v<ivec4>);
static_assert(std::is_trivially_constructible_v<uvec2>);
static_assert(std::is_trivially_constructible_v<uvec3>);
static_assert(std::is_trivially_constructible_v<uvec4>);

}
