/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

implementing lib;

public struct AABB<T:IArithmetic> {
	public vector<T, 2> top_left;
	public vector<T, 2> bottom_right;
};

// Rotate a point clockwise around the origin. Angle in radians.
public func rotate(float2 p, float angle) -> float2
{
	let c = cos(angle);
	let s = sin(angle);
	return float2(p.x * c - p.y * s, p.x * s + p.y * c);
}

// Rotate a point clockwise around an arbitrary point. Angle in radians.
public func rotateAround(float2 p, float2 origin, float angle) -> float2
{ return rotate(p - origin, angle) + origin; }
