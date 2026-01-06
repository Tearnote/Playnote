/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

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

// SDF functions adapted from https://iquilezles.org/articles/distfunctions2d/

// Signed distance from the edge of a pie slice of a circle.
// The pie grows from the bottom in both directions.
// aperture = float2(sin(angle), cos(angle))
public func pie(p: float2, aperture: float2, radius: float) -> float
{
	let sp = float2(abs(p.x), p.y);
	float l = length(sp) - radius;
	float m = length(sp - aperture * clamp(dot(sp, aperture), 0.0, radius));
	return max(l, m * sign(aperture.y * sp.x - aperture.x * sp.y));
}

// Signed distance from an axis-aligned rectangle.
public func rect(p: float2, size: float2) -> float
{
	let d = abs(p) - size / 2.0;
	return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// Signed distance from a polygon of an arbitrary number of vertices.
public func polygon(p: float2, vcount: int, load: IFunc<float2, int>) -> float
{
	var d = dot(p - load(0), p - load(0));
	var s = 1.0;
	for (int i = 0, j = vcount - 1; i < vcount; j = i, i++) {
		let e = load(j) - load(i);
		let w =       p - load(i);
		let b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
		d = min(d, dot(b, b));
		let c = bool3(p.y >= load(i).y, p.y < load(j).y, e.x * w.y > e.y * w.x);
		if (all(c) || all(~c)) s *= -1.0;
	}
	return s * sqrt(d);
}

}
