/*
Copyright (c) 2025 Tearnote (Hubert Maraszek)

Licensed under the Mozilla Public License Version 2.0 <LICENSE-MPL-2.0.txt
or https://www.mozilla.org/en-US/MPL/2.0/> or the Boost Software License <LICENSE-BSL-1.0.txt
or https://www.boost.org/LICENSE_1_0.txt>, at your option. This file may not be copied, modified,
or distributed except according to those terms.
*/

#language slang 2026

import lib;
#include "shared/primitive.slang.h"
#include "shared/worklist.slang.h"

StructuredBuffer<Primitive> b_primitives;
StructuredBuffer<WorklistItem> b_worklists;
StructuredBuffer<int> b_worklistSizes;
RWTexture2D<float4> i_target;

[SpecializationConstant] let ViewportWidth = 0u;
[SpecializationConstant] let ViewportHeight = 0u;
static let ViewportSize = int2(ViewportWidth, ViewportHeight);
static let TileBound = (ViewportSize + TILE_SIZE - 1) / TILE_SIZE;

static let MaxMotionSupersample = 4u;
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

func circle_coverage(circle: Primitive, at: float2) -> float
{
	let params = reinterpret<Primitive::CircleParams>(circle.params);
	let scaled_velocity = circle.velocity * 2;
	let velocity_offset = scaled_velocity / 4;
	let center = at - velocity_offset - circle.position;
	let velocity_len = length(scaled_velocity);

	if (velocity_len > MaxMotionSupersample) {
		// Analytical motion blur with supersampled AA
		var alpha = 0.0;
		for (var i = 0u; i < 4; i += 1) {
			let sample_center = center + (SamplePattern4x[i] - 0.5);
			alpha += motion_blurred_circle_coverage(sample_center, -scaled_velocity, params.radius);
		}
		return alpha / 4;
	} else {
		// Analytical AA with supersampled motion blur
		let sample_count = clamp(uint(ceil(velocity_len)), 1, MaxMotionSupersample);
		let step = sample_count == 1? -scaled_velocity : -scaled_velocity / (sample_count - 1);
		let shutter_step = 1.0 / sample_count;
		var accum_alpha = 0.0;
		for (var i = 0u; i < sample_count; i += 1) {
			let step_center = center - step * i;
			let alpha = sdf::aa(sdf::circle(step_center, params.radius));
			let shutter = cosShutterIntegral(shutter_step * i, shutter_step * (i + 1));
			accum_alpha += alpha * shutter;
		}
		return accum_alpha;
	}
}

[shader("compute")]
[numthreads(16, 16)]
void computeMain(int2 threadId: SV_DispatchThreadID)
{
	if (any(threadId >= ViewportSize)) return;
	let tile = threadId / TILE_SIZE;
	let tileIdx = tile.y * TileBound.x + tile.x;
	let worklistOffset = tileIdx * WORKLIST_MAX_SIZE;
	let worklistSize = b_worklistSizes[tileIdx];
	if (worklistSize <= 0) return;

	let self_pos = float2(threadId) + 0.5;
	var color = i_target.Load(threadId);
	color += float4(0.1, 0.0, 0.0, 1.0);
	color = float4(srgbDecode(color.rgb), color.a);

	for (int i = 0; i < worklistSize; i += 1) {
		let item = b_worklists[worklistOffset + i];
		let primitive = b_primitives[item.primitive_idx];
		switch (primitive.type) {
		case Primitive::Type::Circle:
			let coverage = circle_coverage(primitive, self_pos);
			color = blend(color, float4(primitive.color.rgb, primitive.color.a * coverage));
			break;
		case Primitive::Type::Rect:
			break; //TODO
		}
	}

	color = float4(srgbEncode(color.rgb), color.a);
	i_target.Store(threadId, color);
}
