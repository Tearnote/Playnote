/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

struct Circle {
    float2 position;
    float2 velocity;
    float4 color;
    float radius;
    float _pad0[3];
};

StructuredBuffer<Circle> b_circles;
RWTexture2D<float4> i_target;

static let Pi = 3.14159265359;
[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
[SpecializationConstant] let WindowScale = 0.0;
static let ViewportSize = uint2(ViewportWidth, ViewportHeight);

func blend(onto: float4, color: float4) -> float4
{
    return lerp(onto, color, color.a);
}

func circle_sdf(p: float2, radius: float) -> float
{
    return length(p) - radius;
}

// Antiderivative of the cosine shutter function
// https://www.shadertoy.com/view/cdXSRn
func cos_shutter(t: float) -> float
{
    if (t < 0.0) return 0.0;
    if (t > 1.0) return 1.0;
    return t - sin(2.0 * Pi * t) / (2.0 * Pi);
}

func motion_blurred_circle_coverage(p: float2, v: float2, r: float) -> float
{
    let a = dot(v, v);
    if (a < 0.0001) {
        let dist = length(p) - r;
        return saturate(0.5 - dist);
    }

    let b = -2.0 * dot(p, v);
    let c = dot(p, p) - r * r;

    let delta = b * b - 4.0 * a * c;
    if (delta < 0.0) return 0.0;

    let sqrt_delta = sqrt(delta);
    let t1 = (-b - sqrt_delta) / (2.0 * a);
    let t2 = (-b + sqrt_delta) / (2.0 * a);

    let overlap_start = max(0.0, t1);
    let overlap_end = min(1.0, t2);
    return cos_shutter(overlap_end) - cos_shutter(overlap_start);
}

func draw_circle(circle: Circle, at: float2) -> float4
{
    let center = at - circle.position * WindowScale;
    let alpha = motion_blurred_circle_coverage(center, circle.velocity * WindowScale, circle.radius * WindowScale);
    return circle.color * alpha;
}

[shader("compute")]
[numthreads(16, 16)]
void computeMain(uint2 threadId: SV_DispatchThreadID, uniform uint circle_count, uniform float timer)
{
    if (any(threadId >= ViewportSize)) return;
    let self_pos = float2(threadId) + 0.5;

    var color = i_target.Load(threadId);
    for (int i = 0; i < circle_count; i += 1) {
        let new_color = draw_circle(b_circles[i], self_pos);
        color = blend(color, new_color);
    }
    i_target.Store(threadId, color);
}
