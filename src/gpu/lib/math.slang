/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

implementing lib;

public static let Pi = 3.14159265359;

// Squared length.
public func dot2(v: float2) -> float { return dot(v, v); }
public func dot2(v: float3) -> float { return dot(v, v); }

// Antiderivative of the cosine shutter function.
func cosShutterAntiderivative(t: float) -> float
{
    if (t < 0.0) return 0.0;
    if (t > 1.0) return 1.0;
    return t - sin(2.0 * Pi * t) / (2.0 * Pi);
}

// Integral of the cosine shutter function within a time period.
// The function is roughly bell-shaped, spanning t=[0.0, 1.0]. Requires t1 <= t2.
// https://www.shadertoy.com/view/cdXSRn
public func cosShutterIntegral(t1: float, t2: float) -> float
{
	return cosShutterAntiderivative(t2) - cosShutterAntiderivative(t1);
}
