/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

implementing lib;

// Perform a standard alpha blend of one color onto another.
public func blend(onto: float4, color: float4) -> float4
{
    return lerp(onto, color, color.a);
}

// Encode a linear-space color as sRGB.
// http://www.java-gaming.org/topics/fast-srgb-conversion-glsl-snippet/37583/view.html
public func srgbEncode(color: float3) -> float3
{
    let r = color.r < 0.0031308 ? 12.92 * color.r : 1.055 * pow(color.r, 1.0 / 2.4) - 0.055;
    let g = color.g < 0.0031308 ? 12.92 * color.g : 1.055 * pow(color.g, 1.0 / 2.4) - 0.055;
    let b = color.b < 0.0031308 ? 12.92 * color.b : 1.055 * pow(color.b, 1.0 / 2.4) - 0.055;
    return {r, g, b};
}
