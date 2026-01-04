/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

implementing lib;

public static let Tau = 6.28318530718;

// Squared length.
public func dot2(v: float2) -> float { return dot(v, v); }
public func dot2(v: float3) -> float { return dot(v, v); }

// Equality with an epsilon.
public func almostEqual(a: float, b: float, epsilon: float = 1e-6) -> bool
{ return abs(a - b) < epsilon; }

// Median of three values.
public func median(a: float, b: float, c: float) -> float
{ return max(min(a, c), min(max(a, c), b)); }

// Antiderivative of the cosine shutter function.
func cosShutterAntiderivative(t: float) -> float
{
    if (t < 0.0) return 0.0;
    if (t > 1.0) return 1.0;
    return t - sin(Tau * t) / Tau;
}

// Integral of the cosine shutter function within a time period.
// The function is roughly bell-shaped, spanning t=[0.0, 1.0]. Requires t1 <= t2.
// https://www.shadertoy.com/view/cdXSRn
public func cosShutterIntegral(t1: float, t2: float) -> float
{
	return cosShutterAntiderivative(t2) - cosShutterAntiderivative(t1);
}
