/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/math.hpp:
Imports of math functions.
*/

#pragma once
#include <algorithm>
#include <cmath>
#include <boost/rational.hpp>

namespace playnote {

using std::min;
using std::max;
using std::floor;
using std::ceil;
using std::trunc;
using std::abs;
using std::pow;
using std::exp;
using std::sqrt;
using boost::rational;
using boost::rational_cast;

// Return the integer part of a rational number.
template<typename T>
constexpr auto trunc(rational<T> const& r) -> T { return static_cast<T>(trunc(rational_cast<double>(r))); }

// Return the fractional part of a rational number.
template<typename T>
constexpr auto fract(rational<T> const& r) -> rational<T> { return {r.numerator() % r.denominator(), r.denominator()}; }

}
