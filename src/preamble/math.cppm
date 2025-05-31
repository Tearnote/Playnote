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
export using std::abs;
export using boost::rational;
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

}
