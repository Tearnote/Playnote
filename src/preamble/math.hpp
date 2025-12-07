/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
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
