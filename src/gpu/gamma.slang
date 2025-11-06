/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

RWTexture2D<float4> t_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
static let ViewportSize = uint2(ViewportWidth, ViewportHeight);

// http://www.java-gaming.org/topics/fast-srgb-conversion-glsl-snippet/37583/view.html
func srgbEncode(float3 color) -> float3
{
    let r = color.r < 0.0031308 ? 12.92 * color.r : 1.055 * pow(color.r, 1.0 / 2.4) - 0.055;
    let g = color.g < 0.0031308 ? 12.92 * color.g : 1.055 * pow(color.g, 1.0 / 2.4) - 0.055;
    let b = color.b < 0.0031308 ? 12.92 * color.b : 1.055 * pow(color.b, 1.0 / 2.4) - 0.055;
    return {r, g, b};
}

[shader("compute")]
[numthreads(16, 16)]
void computeMain(uint2 threadId: SV_DispatchThreadID)
{
    if (any(threadId >= ViewportSize)) return;

    let linear = t_target.Load(threadId);
    let srgb = float4(srgbEncode(linear.rgb), linear.a);
    t_target.Store(threadId, srgb);
}
