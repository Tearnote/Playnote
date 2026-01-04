/*
Copyright (c) 2026 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

implementing lib;

// Alpha-blend one color onto another. Assumes premultiplied alpha.
public func blend<int N>(onto: vector<float, N>, color: vector<float, N>) -> vector<float, N>
{ return color + onto * (1.0 - color[color.elementCount - 1]); }

// Additively blend one color onto another. Assumes premultiplied alpha.
public func blendAdditive<int N>(onto: vector<float, N>, color: vector<float, N>) -> vector<float, N>
{
	var result = onto;
	for (var i = 0; i < color.elementCount - 1; i += 1)
		result[i] += color[i];
	return result;
}

// Encode a linear space color as sRGB.
// http://www.java-gaming.org/topics/fast-srgb-conversion-glsl-snippet/37583/view.html
public func srgbEncode(color: float3) -> float3
{
	let r = color.r < 0.0031308 ? 12.92 * color.r : 1.055 * pow(color.r, 1.0 / 2.4) - 0.055;
	let g = color.g < 0.0031308 ? 12.92 * color.g : 1.055 * pow(color.g, 1.0 / 2.4) - 0.055;
	let b = color.b < 0.0031308 ? 12.92 * color.b : 1.055 * pow(color.b, 1.0 / 2.4) - 0.055;
	return {r, g, b};
}

// Decode a color from sRGB to linear space.
public func srgbDecode(color: float3) -> float3
{
	let r = color.r < 0.04045 ? (1.0 / 12.92) * color.r : pow((color.r + 0.055) * (1.0 / 1.055), 2.4);
	let g = color.g < 0.04045 ? (1.0 / 12.92) * color.g : pow((color.g + 0.055) * (1.0 / 1.055), 2.4);
	let b = color.b < 0.04045 ? (1.0 / 12.92) * color.b : pow((color.b + 0.055) * (1.0 / 1.055), 2.4);
	return {r, g, b};
}
