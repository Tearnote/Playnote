/*
This software is dual-licensed. For more details, please consult LICENSE.txt.
Copyright (c) 2025 Tearnote (Hubert Maraszek)

preamble/math.cppm:
Imports of math functions.
*/

module;
#include <algorithm>
#include <cmath>
#include <boost/rational.hpp>

export module playnote.preamble:math;

namespace playnote {

export using std::min;
export using std::max;
export using std::floor;
export using std::ceil;
export using std::trunc;
export using std::abs;
export using std::pow;
export using std::exp;
export using std::sqrt;
export using boost::rational;
export using boost::rational_cast;
export using boost::operator+;
export using boost::operator-;
export using boost::operator*;
export using boost::operator/;
export using boost::operator==;
export using boost::operator!=;
export using boost::operator<;
export using boost::operator>;
export using boost::operator<=;
export using boost::operator>=;

// Return the integer part of a rational number.
export template<typename T>
constexpr auto trunc(rational<T> const& r) -> T { return static_cast<T>(trunc(rational_cast<double>(r))); }

// Return the fractional part of a rational number.
export template<typename T>
constexpr auto fract(rational<T> const& r) -> rational<T> { return {r.numerator() % r.denominator(), r.denominator()}; }

}
