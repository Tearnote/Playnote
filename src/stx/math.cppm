module;
#include <initializer_list>
#include <type_traits>
#include <algorithm>
#include <concepts>
#include <numbers>
#include <ranges>
#include <array>
#include "types.hpp"

export module playnote.stx.math;

namespace playnote::stx {

namespace ranges = std::ranges;

// A built-in type with defined arithmetic operations (+, -, *, /)
template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;

export template<std::floating_point T>
constexpr auto Tau_v = std::numbers::pi_v<T> * 2;
export constexpr auto Tau = Tau_v<float>;

export template<arithmetic T, std::floating_point Prec = float>
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
export template<std::integral T>
[[nodiscard]] constexpr auto tmod(T num, T div) { return num % div + (num % div < 0) * div; }

// GLSL-style scalar clamp
export template<arithmetic T>
[[nodiscard]] constexpr auto clamp(T val, T vmin, T vmax) -> T { return std::max(vmin, std::min(val, vmax)); }

// Generic math vector, of any dimension between 2 to 4 and any underlying type
export template<usize Dim, arithmetic T>
class vec {
public:
	static_assert(Dim >= 2 && Dim <= 4, "Vectors need to have 2, 3 or 4 components");
	using self_t = vec;

	// Uninitialized init
	constexpr vec() = default;
	// Fill the vector with copies of the value
	explicit constexpr vec(T fillVal) { fill(fillVal); }
	// Create the vector with provided component values
	constexpr vec(std::initializer_list<T> list) { ranges::copy(list, arr.begin()); }
	// Variadic version of the above
	template<typename... Args>
	explicit constexpr vec(Args&&... args) { arr = std::to_array({static_cast<T>(args)...}); }

	// Type cast
	template<arithmetic U>
		requires (!std::same_as<T, U>)
	explicit constexpr vec(vec<Dim, U> const& other)
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] = static_cast<T>(other[i]);
	}

	// Dimension downcast
	template<usize N>
		requires (N > Dim)
	explicit constexpr vec(vec<N, T> const& other)
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] = other[i];
	}

	// Dimension upcast
	template<usize N>
		requires (N < Dim)
	constexpr vec(vec<N, T> const& other, T fill)
	{
		arr.fill(fill);
		for (auto i: ranges::iota_view(0uz, N))
			arr[i] = other[i];
	}

	[[nodiscard]] constexpr auto at(usize n) -> T& { return arr[n]; }
	[[nodiscard]] constexpr auto at(usize n) const -> T { return arr[n]; }

	[[nodiscard]] constexpr auto operator[](usize n) -> T& { return at(n); }
	[[nodiscard]] constexpr auto operator[](usize n) const -> T { return at(n); }

	[[nodiscard]] constexpr auto x() -> T&
	{
		static_assert(Dim >= 1);
		return arr[0];
	}
	[[nodiscard]] constexpr auto x() const -> T
	{
		static_assert(Dim >= 1);
		return arr[0];
	}
	[[nodiscard]] constexpr auto y() -> T&
	{
		static_assert(Dim >= 2);
		return arr[1];
	}
	[[nodiscard]] constexpr auto y() const -> T
	{
		static_assert(Dim >= 2);
		return arr[1];
	}
	[[nodiscard]] constexpr auto z() -> T&
	{
		static_assert(Dim >= 3);
		return arr[2];
	}
	[[nodiscard]] constexpr auto z() const -> T
	{
		static_assert(Dim >= 3);
		return arr[2];
	}
	[[nodiscard]] constexpr auto w() -> T&
	{
		static_assert(Dim >= 4);
		return arr[3];
	}
	[[nodiscard]] constexpr auto w() const -> T
	{
		static_assert(Dim >= 4);
		return arr[3];
	}

	[[nodiscard]] constexpr auto r() -> T& { return x(); }
	[[nodiscard]] constexpr auto r() const -> T { return x(); }
	[[nodiscard]] constexpr auto g() -> T& { return y(); }
	[[nodiscard]] constexpr auto g() const -> T { return y(); }
	[[nodiscard]] constexpr auto b() -> T& { return z(); }
	[[nodiscard]] constexpr auto b() const -> T { return z(); }
	[[nodiscard]] constexpr auto a() -> T& { return w(); }
	[[nodiscard]] constexpr auto a() const -> T { return w(); }

	constexpr void fill(T val) { arr.fill(val); }

	constexpr auto operator+=(self_t const& other) -> self_t&
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] += other[i];
		return *this;
	}

	constexpr auto operator-=(self_t const& other) -> self_t&
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] -= other[i];
		return *this;
	}

	constexpr auto operator*=(self_t const& other) -> self_t&
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] *= other[i];
		return *this;
	}

	constexpr auto operator/=(self_t const& other) -> self_t&
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] /= other[i];
		return *this;
	}

	constexpr auto operator%=(self_t const& other) -> self_t&
	{
		static_assert(std::is_integral_v<T>);

		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] %= other[i];
		return *this;
	}

	// Scalar arithmetic

	constexpr auto operator*=(T other) -> self_t&
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] *= other;
		return *this;
	}

	constexpr auto operator/=(T other) -> self_t&
	{
		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] /= other;
		return *this;
	}

	constexpr auto operator%=(T other) -> self_t&
	{
		static_assert(std::is_integral_v<T>);

		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] %= other;
		return *this;
	}

	constexpr auto operator<<=(T other) -> self_t&
	{
		static_assert(std::is_integral_v<T>);

		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] <<= other;
		return *this;
	}

	constexpr auto operator>>=(T other) -> self_t&
	{
		static_assert(std::is_integral_v<T>);

		for (auto i: ranges::iota_view(0uz, Dim))
			arr[i] >>= other;
		return *this;
	}

private:
	std::array<T, Dim> arr;
};

export template<usize Dim, arithmetic T>
constexpr auto operator+(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result += right;
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto operator-(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result -= right;
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto operator*(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result *= right;
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto operator/(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result /= right;
	return result;
}

export template<usize Dim, std::integral T>
constexpr auto operator%(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = left;
	result %= right;
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto operator==(vec<Dim, T> const& left, vec<Dim, T> const& right) -> bool
{
	for (auto i: ranges::iota_view(0uz, Dim)) {
		if (left[i] != right[i])
			return false;
	}
	return true;
}

export template<usize Dim, arithmetic T>
constexpr auto min(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = vec<Dim, T>();
	for (auto i: ranges::iota_view(0uz, Dim))
		result[i] = std::min(left[i], right[i]);
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto max(vec<Dim, T> const& left, vec<Dim, T> const& right) -> vec<Dim, T>
{
	auto result = vec<Dim, T>();
	for (auto i: ranges::iota_view(0uz, Dim))
		result[i] = std::max(left[i], right[i]);
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto operator*(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result *= right;
	return result;
}

export template<usize Dim, arithmetic T>
constexpr auto operator*(T left, vec<Dim, T> const& right) -> vec<Dim, T> { return right * left; }

export template<usize Dim, arithmetic T>
constexpr auto operator/(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result /= right;
	return result;
}

export template<usize Dim, std::integral T>
constexpr auto operator%(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result %= right;
	return result;
}

export template<usize Dim, std::integral T>
constexpr auto operator<<(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result <<= right;
	return result;
}

export template<usize Dim, std::integral T>
constexpr auto operator>>(vec<Dim, T> const& left, T right) -> vec<Dim, T>
{
	auto result = left;
	result >>= right;
	return result;
}

// Component-wise absolute value
export template<usize Dim, std::floating_point T>
constexpr auto abs(vec<Dim, T> const& v) -> vec<Dim, T>
{
	auto result = vec<Dim, T>{};
	for (auto i: ranges::iota_view(0uz, Dim))
		result[i] = abs(v[i]);
	return result;
}

export using vec2 = vec<2, float>;
export using vec3 = vec<3, float>;
export using vec4 = vec<4, float>;
export using ivec2 = vec<2, int>;
export using ivec3 = vec<3, int>;
export using ivec4 = vec<4, int>;
export using uvec2 = vec<2, uint>;
export using uvec3 = vec<3, uint>;
export using uvec4 = vec<4, uint>;

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
