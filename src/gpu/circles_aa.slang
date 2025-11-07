/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

import lib;

struct Circle {
    float2 position;
    float2 velocity;
    float4 color;
    float radius;
    float _pad0[3];
};

StructuredBuffer<Circle> b_circles;
RWTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let WindowScale = 0.0;
static let ViewportSize = uint2(ViewportWidth, ViewportHeight);

func draw_circle(circle: Circle, at: float2) -> float4
{
    let center = at - circle.position * WindowScale;
    let alpha = sdf::aa(sdf::circle(center, circle.radius * WindowScale));
    return circle.color * alpha;
}

[shader("compute")]
[numthreads(16, 16)]
void computeMain(uint2 threadId: SV_DispatchThreadID, uniform uint circle_count, uniform float timer)
{
    if (any(threadId >= ViewportSize)) return;
    let self_pos = float2(threadId) + 0.5;

    var color = i_target.Load(threadId);
    color = float4(srgbDecode(color.rgb), color.a);

    for (int i = 0; i < circle_count; i += 1) {
        let new_color = draw_circle(b_circles[i], self_pos);
        color = blend(color, new_color);
    }

    color = float4(srgbEncode(color.rgb), color.a);
    i_target.Store(threadId, color);
}
