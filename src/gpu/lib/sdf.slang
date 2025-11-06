/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

implementing lib;

namespace sdf {

// Antialias a distance value.
public func aa(dist: float) -> float {
	return saturate(0.5 - dist);
}

// Signed distance from the edge of a circle.
public func circle(p: float2, radius: float) -> float
{
    return length(p) - radius;
}

}
