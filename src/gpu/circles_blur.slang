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
static let ViewportSize: uint2 = {ViewportWidth, ViewportHeight};

static let SamplePattern4x: float2[4] = {
	{0.375, 0.125},
	{0.875, 0.375},
	{0.125, 0.625},
	{0.625, 0.875}
};

func motion_blurred_circle_coverage(p: float2, v: float2, r: float) -> float
{
    let a = dot2(v);
    if (a < 0.001) return sdf::circle(p, r) <= 0.0? 1.0 : 0.0;
    let b = -2.0 * dot(p, v);
    let c = dot2(p) - r * r;

    let delta = b * b - 4.0 * a * c;
    if (delta < 0.0) return 0.0;

    let sqrt_delta = sqrt(delta);
    let t1 = (-b - sqrt_delta) / (2.0 * a);
    let t2 = (-b + sqrt_delta) / (2.0 * a);

    let overlap_start = max(0.0, t1);
    let overlap_end = min(1.0, t2);
    return cosShutterIntegral(overlap_start, overlap_end);
}

func draw_circle(circle: Circle, at: float2) -> float4
{
	var alpha = 0.0;
	for (var i = 0u; i < 4; i += 1) {
		let center = at + (SamplePattern4x[i] - 0.5) - circle.position * WindowScale;
	    alpha += motion_blurred_circle_coverage(center, circle.velocity * WindowScale, circle.radius * WindowScale);
	}
	return circle.color * (alpha / 4);
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
