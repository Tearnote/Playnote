/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#pragma once
#include <chrono> // IWYU pragma: export

namespace playnote {

using std::literals::operator ""ns;
using std::literals::operator ""ms;
using std::literals::operator ""s;
using std::chrono::nanoseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::abs;

// Returns the ratio of two durations as a floating-point number.
template<typename LRep, typename LPeriod, typename RRep, typename RPeriod>
auto ratio(duration<LRep, LPeriod> num, duration<RRep, RPeriod> denom) -> double
{
	return duration_cast<duration<double, LPeriod>>(num) / duration_cast<duration<double, RPeriod>>(denom);
}

}
